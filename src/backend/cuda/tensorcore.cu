/*
 * Execute admitted low-bit rows by Q8_K activations through native integer Tensor Cores.
 *
 * Encoded scales remain outside MMA. Each 16-element segment is expanded directly into an
 * integer fragment, so Q8_0, Q2_K, IQ2_XXS, and MXFP4 keep their canonical block semantics
 * without materializing a dense dequantized matrix.
 */
#include "src/backend/cuda/kernel_primitives.h"
#include "src/backend/cuda/dot_recovery.h"
#include <yvex/internal/execution_batch.h>

static __device__ int tensorcore_row_geometry(unsigned int qtype,
                                              unsigned long long width,
                                              unsigned long long row_bytes)
{
    if (!width || width % YVEX_CUDA_Q8_K_BLOCK) return 0;
    if (qtype == YVEX_GGUF_QTYPE_Q8_0)
        return row_bytes == (width / 32ull) * 34ull;
    if (qtype == YVEX_GGUF_QTYPE_Q2_K)
        return row_bytes == (width / 256ull) * 84ull;
    if (qtype == YVEX_GGUF_QTYPE_IQ2_XXS)
        return row_bytes == (width / 256ull) * 66ull;
    if (qtype == YVEX_GGUF_QTYPE_MXFP4)
        return row_bytes == (width / 32ull) * 17ull;
    return 0;
}

static __device__ float tensorcore_scaled_group(const unsigned char *row,
                                                unsigned long long segment,
                                                unsigned int qtype,
                                                float activation_scale,
                                                int product, int minimum)
{
    if (qtype == YVEX_GGUF_QTYPE_Q8_0) {
        const unsigned char *block = row + (segment / 32ull) * 34ull;
        return activation_scale * f16_bits_to_float(qtype_load_u16(block)) *
               (float)product;
    }
    if (qtype == YVEX_GGUF_QTYPE_Q2_K) {
        const unsigned char *block = row + (segment / 256ull) * 84ull;
        return activation_scale * f16_bits_to_float(qtype_load_u16(block + 80u)) *
                   (float)product -
               activation_scale * f16_bits_to_float(qtype_load_u16(block + 82u)) *
                   (float)minimum;
    }
    if (qtype == YVEX_GGUF_QTYPE_IQ2_XXS) {
        const unsigned char *block = row + (segment / 256ull) * 66ull;
        return 0.125f * f16_bits_to_float(qtype_load_u16(block)) *
               activation_scale * (float)product;
    }
    const unsigned char *block = row + (segment / 32ull) * 17ull;
    return 0.5f * activation_scale * e8m0_bits_to_float(block[0]) *
           (float)product;
}

/* Expand the immutable magnitude codebook once per block. Sign application
 * remains per encoded quartet and cannot change the IQ2 scale or population. */
static __device__ void tensorcore_iq2_grid(unsigned int *table) {
    for (unsigned i = threadIdx.x; i < 256u; i += blockDim.x) {
        unsigned short grid = iq2_xxs_grid[i];
        for (unsigned part = 0u; part < 2u; ++part) {
            unsigned packed = 0u;
            for (unsigned j = 0u; j < 4u; ++j) {
                unsigned code = (grid >> (2u * (part * 4u + j))) & 3u;
                packed |= (code == 0u ? 8u : code == 1u ? 25u : 43u) << (8u * j);
            }
            table[i * 2u + part] = packed;
        }
    }
}
static __device__ int tensorcore_expert_digit4(const unsigned char *row, unsigned block,
    unsigned first, unsigned qtype, const unsigned int *grid_table = nullptr) {
    if(qtype==YVEX_GGUF_QTYPE_Q2_K) {
        const unsigned char *p=row+block*84;
        unsigned packed=qtype_load_u32(p+16+(first/128)*32+(first%32));
        unsigned shift=(first%128)/32*2;
        unsigned codes=(packed>>shift)&0x03030303u;
        return (int)(codes*(p[first/16]&15));
    }
    unsigned result=0;
    const unsigned char *p=row+block*66;
    unsigned group=first/32, subgroup=first%32/8;
    unsigned grids=qtype_load_u32(p+2+group*8), ss=qtype_load_u32(p+6+group*8);
    unsigned grid_index=(grids>>(subgroup*8))&255;
    unsigned signs=iq2_xxs_signs((ss>>(subgroup*7))&127);
    if (grid_table) {
        unsigned part = (first & 4u) >> 2u;
        unsigned packed = grid_table[grid_index * 2u + part];
        unsigned bits = (signs >> (part * 4u)) & 15u;
        unsigned mask = ((bits * 0x00204081u) & 0x01010101u) * 255u;
        return (int)__vsub4(packed ^ mask, mask);
    }
    unsigned short grid=iq2_xxs_grid[grid_index];
    for(unsigned i=0;i<4;i++) {
        unsigned at=first%8+i, code=(grid>>(2*at))&3;
        int v=code==0?8:code==1?25:43;
        if(signs&(1u<<at))v=-v;
        result|=((unsigned)v&255)<<(8*i);
    }
    return (int)result;
}
static __device__ void tensorcore_integer_mma(int c[4], const int a[4], const int b[2]) {
    asm volatile("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(c[0]),"+r"(c[1]),"+r"(c[2]),"+r"(c[3])
        : "r"(a[0]),"r"(a[1]),"r"(a[2]),"r"(a[3]),"r"(b[0]),"r"(b[1]));
}
/* Register MMA owns sixteen rows by eight inputs per warp. Integer products
 * retain each format's exact 32/256-element block; F32 block scales and their
 * source-order sum are unchanged. No shared fragments or block barriers are
 * needed for these independent output tiles. */
static __device__ int tensorcore_plain_digit4(const unsigned char *row, unsigned long long segment,
                                 unsigned first, unsigned qtype) {
    if (qtype == YVEX_GGUF_QTYPE_Q8_0)
        return (int)qtype_load_u32(row + (segment/32)*34 + 2 + first);
    unsigned packed = qtype_load_u32(row + (segment/32)*17 + 1 + first%16);
    return mxfp4_i8x4(packed, first >= 16u);
}
extern "C" __global__ void yvex_qtype_tensorcore_rows(
    const unsigned char *encoded, unsigned long long row_bytes,
    unsigned long long row_width, unsigned long long start_row,
    unsigned long long row_count, unsigned long long input_rows,
    unsigned long long group_count, unsigned long long group_rows,
    unsigned qtype, const unsigned char *activation, const float *additive,
    float *output, int output_bf16, int *status) {
    __shared__ unsigned int grid_table[512];
    if (qtype == YVEX_GGUF_QTYPE_IQ2_XXS) {
        tensorcore_iq2_grid(grid_table);
        __syncthreads();
    }
    unsigned lane=threadIdx.x&31, warp=threadIdx.x>>5, warps=blockDim.x>>5;
    if (!status || *status) return;
    if(!encoded || !activation || !output || !row_count || !input_rows || !group_count ||
       !group_rows || group_count>~0ull/group_rows || row_count!=group_count*group_rows ||
       input_rows>~0ull-15 || !warps || warps>4 || blockDim.x!=warps*32 ||
       (group_count>1 && group_rows%16) ||
       !tensorcore_row_geometry(qtype,row_width,row_bytes)) {
       if(!threadIdx.x)atomicCAS(status,0,2);return;
    }
    unsigned long long input_groups=(((input_rows+7)/8)+warps-1)/warps;
    unsigned long long row_base=((unsigned long long)blockIdx.x/input_groups)*16;
    unsigned long long input_base=((unsigned long long)blockIdx.x%input_groups)*warps*8+warp*8;
    unsigned long long row_group=row_base/group_rows, blocks=row_width/256;
    if(row_group>=group_count){if(!threadIdx.x)atomicCAS(status,0,2);return;}
    for(unsigned half=0;half<1;half++) {
        float totals[4]={};
        unsigned extent=(qtype==YVEX_GGUF_QTYPE_IQ2_XXS || qtype==YVEX_GGUF_QTYPE_Q2_K)?256:32;
        for(unsigned long long group_start=0;group_start<row_width;group_start+=extent) {
            int product[4]={};
            for(unsigned long long segment=group_start;segment<group_start+extent;segment+=32) {
            int a[4],b[2];
            for(unsigned j=0;j<4;j++) {
                unsigned long long row=row_base+(lane>>2)+(j&1)*8;
                unsigned first=(lane&3)*4+(j>>1)*16;
                const unsigned char *w=encoded+(start_row+row)*row_bytes;
                a[j]=row<row_count?(extent==32?tensorcore_plain_digit4(w,segment,first,qtype):
                    tensorcore_expert_digit4(w,segment/256,segment%256+first,qtype,grid_table)):0;
            }
            unsigned long long column=input_base+half*8+(lane>>2);
            for(unsigned j=0;j<2;j++)b[j]=column<input_rows?(int)qtype_load_u32(activation+
                ((column*group_count+row_group)*blocks+segment/256)*YVEX_CUDA_Q8_K_BYTES+
                4+segment%256+(lane&3)*4+j*16):0;
            if(qtype==YVEX_GGUF_QTYPE_IQ2_XXS) {
                int segment_product[4]={};tensorcore_integer_mma(segment_product,a,b);
                for(unsigned j=0;j<4;j++) {
                    unsigned long long row=row_base+(lane>>2)+(j>>1)*8;
                    unsigned factor=row<row_count?2*(encoded[(start_row+row)*row_bytes+
                        (segment/256)*66+9+(segment%256/32)*8]>>4)+1:0;
                    product[j]+=segment_product[j]*(int)factor;
                }
            } else tensorcore_integer_mma(product,a,b);
            }
            for(unsigned j=0;j<4;j++) {
                unsigned long long row=row_base+(lane>>2)+(j>>1)*8;
                unsigned long long col=input_base+half*8+(lane&3)*2+(j&1);
                if(row<row_count && col<input_rows) {
                    const unsigned char *q8=activation+
                        ((col*group_count+row_group)*blocks+group_start/256)*YVEX_CUDA_Q8_K_BYTES;
                    const unsigned char *w=encoded+(start_row+row)*row_bytes;
                    int minimum=0;
                    if(qtype==YVEX_GGUF_QTYPE_Q2_K)
                        for(unsigned s=0;s<16;s++)minimum+=q8_k_sum(q8,s)*(w[group_start/256*84+s]>>4);
                    totals[j]=__fadd_rn(totals[j],tensorcore_scaled_group(w,
                        group_start,qtype,__uint_as_float(qtype_load_u32(q8)),product[j],minimum));
                }
            }
        }
        for(unsigned j=0;j<4;j++) {
            unsigned long long row=row_base+(lane>>2)+(j>>1)*8;
            unsigned long long col=input_base+half*8+(lane&3)*2+(j&1);
            if(row<row_count && col<input_rows) {
                unsigned long long i=col*row_count+row;
                float value=additive?__fadd_rn(totals[j],additive[i]):totals[j];
                if(!isfinite(value))atomicCAS(status,0,1);
                else output[i]=output_bf16?float_to_bf16_rne(value):value;
            }
        }
    }
}

/* Broad projections reuse each decoded weight fragment across two independent
 * eight-input MMA tiles. Per-output integer products, scale application and
 * source-ordered F32 block sums remain identical to the narrow kernel. */
/* Select an admitted encoded format before the hot loop. The fixed-format
 * instantiation removes unused decoders and their register/shared-memory
 * footprint; both entrypoints share this single numerical implementation. */
template<unsigned QTYPE>
static __device__ __forceinline__ void tensorcore_wide_rows(
    const unsigned char *encoded,unsigned long long row_bytes,unsigned long long row_width,
    unsigned long long start_row,unsigned long long row_count,unsigned long long input_rows,
    unsigned long long group_count,unsigned long long group_rows,unsigned supplied_qtype,
    const unsigned char *activation,const float *additive,float *output,int output_bf16,int *status)
{
    constexpr unsigned N=2;
    const unsigned qtype = QTYPE ? QTYPE : supplied_qtype;
    if (QTYPE && supplied_qtype != QTYPE) {
        if (!threadIdx.x && status) atomicCAS(status, 0, 2);
        return;
    }
    __shared__ unsigned grid_table[512];
    if(qtype==YVEX_GGUF_QTYPE_IQ2_XXS) {tensorcore_iq2_grid(grid_table);__syncthreads();}
    unsigned lane=threadIdx.x&31,warp=threadIdx.x>>5,warps=blockDim.x>>5;
    if (!status || *status) return;
    if (!encoded || !activation || !output || !row_count || !input_rows ||
        !group_count || !group_rows || group_count > ~0ull / group_rows ||
        row_count != group_count * group_rows || input_rows > ~0ull - 15ull ||
        !warps || warps > 4 || blockDim.x != warps * 32 ||
        (group_count > 1 && group_rows % 16) ||
        !tensorcore_row_geometry(qtype, row_width, row_bytes)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long input_groups=((input_rows+N*8-1)/(N*8)+warps-1)/warps;
    unsigned long long row_base=((unsigned long long)blockIdx.x/input_groups)*16;
    unsigned long long input_base=((unsigned long long)blockIdx.x%input_groups)*warps*N*8+warp*N*8;
    unsigned long long row_group=row_base/group_rows,blocks=row_width/256;
    if(row_base>=row_count || input_base>=input_rows)return;
    float totals[N][4]={};
    unsigned extent=(qtype==YVEX_GGUF_QTYPE_IQ2_XXS || qtype==YVEX_GGUF_QTYPE_Q2_K)?256:32;
    for(unsigned long long group_start=0;group_start<row_width;group_start+=extent) {
        int product[N][4]={};
        for(unsigned long long segment=group_start;segment<group_start+extent;segment+=32) {
            int a[4];
            #pragma unroll
            for(unsigned j=0;j<4;j++) {
                unsigned long long row=row_base+(lane>>2)+(j&1)*8;
                unsigned first=(lane&3)*4+(j>>1)*16;
                const unsigned char *w=encoded+(start_row+row)*row_bytes;
                a[j]=row<row_count?(extent==32?tensorcore_plain_digit4(w,segment,first,qtype):
                    tensorcore_expert_digit4(w,segment/256,segment%256+first,qtype,grid_table)):0;
            }
            #pragma unroll
            for(unsigned tile=0;tile<N;tile++) {
                int b[2];unsigned long long column=input_base+tile*8+(lane>>2);
                #pragma unroll
                for(unsigned j=0;j<2;j++)b[j]=column<input_rows?(int)qtype_load_u32(activation+
                    ((column*group_count+row_group)*blocks+segment/256)*YVEX_CUDA_Q8_K_BYTES+
                    4+segment%256+(lane&3)*4+j*16):0;
                if(qtype==YVEX_GGUF_QTYPE_IQ2_XXS) {
                    int sp[4]={};tensorcore_integer_mma(sp,a,b);
                    #pragma unroll
                    for(unsigned j=0;j<4;j++) {
                        unsigned long long row=row_base+(lane>>2)+(j>>1)*8;
                        unsigned factor=row<row_count?2*(encoded[(start_row+row)*row_bytes+
                            (segment/256)*66+9+(segment%256/32)*8]>>4)+1:0;
                        product[tile][j]+=sp[j]*(int)factor;
                    }
                } else tensorcore_integer_mma(product[tile],a,b);
            }
        }
        #pragma unroll
        for(unsigned tile=0;tile<N;tile++) {
            #pragma unroll
            for(unsigned j=0;j<4;j++) {
                unsigned long long row=row_base+(lane>>2)+(j>>1)*8;
                unsigned long long col=input_base+tile*8+(lane&3)*2+(j&1);
                if(row<row_count && col<input_rows) {
                    const unsigned char *q8=activation+
                        ((col*group_count+row_group)*blocks+group_start/256)*YVEX_CUDA_Q8_K_BYTES;
                    const unsigned char *w=encoded+(start_row+row)*row_bytes;
                    int minimum=0;
                    if(qtype==YVEX_GGUF_QTYPE_Q2_K)
                        for(unsigned s=0;s<16;s++)minimum+=q8_k_sum(q8,s)*(w[group_start/256*84+s]>>4);
                    totals[tile][j]=__fadd_rn(totals[tile][j],tensorcore_scaled_group(w,
                        group_start,qtype,__uint_as_float(qtype_load_u32(q8)),product[tile][j],minimum));
                }
            }
        }
    }
    #pragma unroll
    for(unsigned tile=0;tile<N;tile++) {
        #pragma unroll
        for(unsigned j=0;j<4;j++) {
            unsigned long long row=row_base+(lane>>2)+(j>>1)*8;
            unsigned long long col=input_base+tile*8+(lane&3)*2+(j&1);
            if(row<row_count && col<input_rows) {
                unsigned long long i=col*row_count+row;
                float value=additive?__fadd_rn(totals[tile][j],additive[i]):totals[tile][j];
                if(!isfinite(value))atomicCAS(status,0,1);
                else output[i]=output_bf16?float_to_bf16_rne(value):value;
            }
        }
    }
}

extern "C" __global__ void yvex_qtype_tensorcore_wide_rows(
    const unsigned char *encoded, unsigned long long row_bytes, unsigned long long row_width,
    unsigned long long start_row, unsigned long long row_count, unsigned long long input_rows,
    unsigned long long group_count, unsigned long long group_rows, unsigned qtype,
    const unsigned char *activation, const float *additive, float *output, int output_bf16, int *status)
{
    tensorcore_wide_rows<0>(encoded, row_bytes, row_width, start_row, row_count, input_rows,
        group_count, group_rows, qtype, activation, additive, output, output_bf16, status);
}

extern "C" __global__ void yvex_mxfp4_tensorcore_wide_rows(
    const unsigned char *encoded, unsigned long long row_bytes, unsigned long long row_width,
    unsigned long long start_row, unsigned long long row_count, unsigned long long input_rows,
    unsigned long long group_count, unsigned long long group_rows, unsigned qtype,
    const unsigned char *activation, const float *additive, float *output, int output_bf16, int *status)
{
    tensorcore_wide_rows<YVEX_GGUF_QTYPE_MXFP4>(encoded, row_bytes, row_width, start_row, row_count,
        input_rows, group_count, group_rows, qtype, activation, additive, output, output_bf16, status);
}

static __device__ float4 tensorcore_f32x4_add(float4 a,float4 b) {
    return make_float4(__fadd_rn(a.x,b.x),__fadd_rn(a.y,b.y),__fadd_rn(a.z,b.z),__fadd_rn(a.w,b.w));
}
/* The explicit row-reduction class uses eight ordered FMAs per Q8_K block,
 * followed by the original 32-lane reduction tree. It is not the ordinary
 * projection's serial scaled-block sum. Integer MMA replaces only the exact
 * 32-element products; bit-reversed leaves preserve that tree verbatim. */
static __device__ float4 tensorcore_expert_register_dot(const unsigned char *weights,
    unsigned long long rb, unsigned long long rows, unsigned long long rowbase,
    const unsigned char *input, unsigned blocks, const unsigned long long *order,
    unsigned long long offset, unsigned pop, unsigned topk, unsigned columnbase,
    unsigned qtype, int ordered, int *status, const unsigned int *grid_table);

extern "C" __global__ void yvex_q8_row_matrix(
    const unsigned char *weights, unsigned long long row_bytes, unsigned long long width,
    unsigned long long start, unsigned long long rows, unsigned long long inputs,
    unsigned qtype, const unsigned char *activation, unsigned long long stride,
    int q8, int block_row, int forensic, const float *additive, float *output,
    unsigned long long output_stride, int output_bf16, int *status)
{
    if (!status || *status) return;
    if (!weights || !activation || !output || !width || width > 8192ull ||
        width % 256ull || row_bytes != width / 256ull *
            (qtype == YVEX_GGUF_QTYPE_Q2_K ? 84ull : 136ull) ||
        !rows || rows > 131072ull || start > ~0ull - rows ||
        (start + rows) > ~0ull / row_bytes || !inputs || inputs > 1024ull ||
        (qtype != YVEX_GGUF_QTYPE_MXFP4 && qtype != YVEX_GGUF_QTYPE_Q2_K) ||
        stride != width || q8 != 1 ||
        block_row || forensic || output_stride < rows || output_stride > ~0ull / inputs ||
        blockDim.x < 32u || blockDim.x > 128u || blockDim.x % 32u ||
        (output_bf16 != 0 && output_bf16 != 1)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    unsigned columns = blockDim.x / 32u * 8u;
    unsigned groups = (unsigned)(inputs + columns - 1ull) / columns, blocks = (unsigned)width / 256u;
    unsigned long long row_base = (unsigned long long)(blockIdx.x / groups) * 16ull;
    unsigned long long input_base = (blockIdx.x % groups) * columns + warp * 8ull;
    if (row_base >= rows || input_base >= inputs) return;
    float4 s0 = {}, s1 = {}, s2 = {}, s3 = {}, s4 = {}, result = {};
    if (qtype == YVEX_GGUF_QTYPE_Q2_K) {
        /* Independent columns reuse encoded weights through the same integer
         * products and 32-leaf F32 tree as the ordinary Q2_K/Q8 row dot. */
        result = tensorcore_expert_register_dot(weights + start * row_bytes, row_bytes,
            rows, row_base, activation, blocks, nullptr, 0ull, (unsigned)inputs,
            1u, (unsigned)input_base, qtype, 1, status, nullptr);
    } else for (unsigned leaf = 0u; leaf < 32u; ++leaf) {
        unsigned block = __brev(leaf) >> 27u;
        float values[4] = {};
        if (block < blocks) {
            for (unsigned part = 0u; part < 8u; ++part) {
                int a[4], b[2], products[4] = {};
                for (unsigned j = 0u; j < 4u; ++j) {
                    unsigned long long row = row_base + (lane >> 2u) + (j & 1u) * 8ull;
                    const unsigned char *w = weights + (start + row) * row_bytes +
                        (block * 8u + part) * 17u + 1u + (lane & 3u) * 4u;
                    a[j] = row < rows ? mxfp4_i8x4(qtype_load_u32(w), j >> 1u) : 0;
                }
                unsigned long long column = input_base + (lane >> 2u);
                for (unsigned j = 0u; j < 2u; ++j)
                    b[j] = column < inputs ? (int)qtype_load_u32(activation +
                        (column * blocks + block) * 292ull + 4u + part * 32u +
                        (lane & 3u) * 4u + j * 16u) : 0;
                tensorcore_integer_mma(products, a, b);
                for (unsigned j = 0u; j < 4u; ++j) {
                    unsigned long long row = row_base + (lane >> 2u) + (j >> 1u) * 8ull;
                    unsigned long long col = input_base + (lane & 3u) * 2u + (j & 1u);
                    if (row < rows && col < inputs) {
                        float ws = e8m0_bits_to_float(weights[(start + row) * row_bytes +
                            (block * 8u + part) * 17u]);
                        float as = __uint_as_float(qtype_load_u32(activation +
                            (col * blocks + block) * 292ull));
                        values[j] = fmaf(ws * 0.5f * as, (float)products[j], values[j]);
                    }
                }
            }
        }
        float4 value = make_float4(values[0], values[1], values[2], values[3]);
        if (!(leaf & 1u)) s0 = value;
        else { value = tensorcore_f32x4_add(s0, value); if (!(leaf & 2u)) s1 = value;
        else { value = tensorcore_f32x4_add(s1, value); if (!(leaf & 4u)) s2 = value;
        else { value = tensorcore_f32x4_add(s2, value); if (!(leaf & 8u)) s3 = value;
        else { value = tensorcore_f32x4_add(s3, value); if (!(leaf & 16u)) s4 = value;
        else result = tensorcore_f32x4_add(s4, value); }}}}
    }
    float values[] = {result.x, result.y, result.z, result.w};
    for (unsigned j = 0u; j < 4u; ++j) {
        unsigned long long row = row_base + (lane >> 2u) + (j >> 1u) * 8ull;
        unsigned long long col = input_base + (lane & 3u) * 2u + (j & 1u);
        if (row < rows && col < inputs) {
            float value = values[j];
            if (!isfinite(value)) value = qtype_q8_dot_recover_f64(
                weights + (start + row) * row_bytes, activation + col * blocks * 292ull,
                blocks, qtype == YVEX_GGUF_QTYPE_Q2_K ? 84ull : 136ull, qtype, status);
            if (additive) value = __fadd_rn(value, additive[col * output_stride + row]);
            if (output_bf16) value = float_to_bf16_rne(value);
            if (!isfinite(value)) atomicCAS(status, 0, 1);
            else output[col * output_stride + row] = value;
        }
    }
}
static __device__ float4 tensorcore_expert_register_dot(const unsigned char *weights,unsigned long long rb,
    unsigned long long rows,unsigned long long rowbase,const unsigned char *input,
    unsigned blocks,const unsigned long long *order,unsigned long long offset,
    unsigned pop,unsigned topk,unsigned columnbase,unsigned qtype,int ordered,int *status,
    const unsigned int *grid_table = nullptr) {
    unsigned lane=threadIdx.x&31, group=lane>>2, t=lane&3;
    float4 s0={},s1={},s2={},s3={},s4={},result={};
    for(unsigned leaf=0;leaf<32;leaf++) {
        unsigned block=__brev(leaf)>>27;
        float4 v={};
        if(block<blocks) {
            int lo[4]={};
            for(unsigned segment=0;segment<256;segment+=32) {
                int a[4],b[2];
                for(unsigned j=0;j<4;j++) {
                    unsigned long long row=rowbase+group+(j&1)*8;
                    unsigned at=segment+t*4+(j>>1)*16;
                    a[j]=row<rows?tensorcore_expert_digit4(weights+row*rb,block,at,qtype,grid_table):0;
                }
                unsigned column=columnbase+group;
                for(unsigned j=0;j<2;j++) {
                    unsigned long long source=column<pop?(ordered?offset+column:order[offset+column]/topk):0;
                    b[j]=column<pop?(int)qtype_load_u32(input+(source*blocks+block)*292+4+segment+t*4+j*16):0;
                }
                /* IQ2's odd scale is common to each 32-column group. Apply
                 * it once to the exact MMA integer result, not separately to
                 * every signed level in two digit planes. The complete
                 * 256-column integer sum and subsequent F32 tree are exact. */
                if(qtype==YVEX_GGUF_QTYPE_IQ2_XXS) {
                    int segment_product[4]={};tensorcore_integer_mma(segment_product,a,b);
                    for(unsigned j=0;j<4;j++) {
                        unsigned long long row=rowbase+group+(j>>1)*8;
                        unsigned factor=row<rows?2*(weights[row*rb+block*66+9+(segment/32)*8]>>4)+1:0;
                        lo[j]+=segment_product[j]*(int)factor;
                    }
                } else tensorcore_integer_mma(lo,a,b);
            }
            float values[4];
            for(unsigned j=0;j<4;j++) {
                unsigned long long row=rowbase+group+(j>>1)*8;
                unsigned column=columnbase+t*2+(j&1);
                float value=0;
                if(row<rows&&column<pop) {
                    unsigned long long source=ordered?offset+column:order[offset+column]/topk;
                    const unsigned char *w=weights+row*rb+block*(qtype==YVEX_GGUF_QTYPE_IQ2_XXS?66:84);
                    const unsigned char *a=input+(source*blocks+block)*292;
                    float as=__uint_as_float(qtype_load_u32(a));
                    if(qtype==YVEX_GGUF_QTYPE_IQ2_XXS)
                        value=0.125f*f16_bits_to_float(qtype_load_u16(w))*as*(float)lo[j];
                    else {
                        int minimum=0;
                        for(unsigned s=0;s<16;s++)minimum+=q8_k_sum(a,s)*(int)(w[s]>>4);
                        value=as*f16_bits_to_float(qtype_load_u16(w+80))*(float)lo[j]-
                            as*f16_bits_to_float(qtype_load_u16(w+82))*(float)minimum;
                    }
                }
                values[j]=value;
            }
            v=make_float4(values[0],values[1],values[2],values[3]);
        }
        if(!(leaf&1))s0=v;
        else {v=tensorcore_f32x4_add(s0,v);if(!(leaf&2))s1=v;
        else {v=tensorcore_f32x4_add(s1,v);if(!(leaf&4))s2=v;
        else {v=tensorcore_f32x4_add(s2,v);if(!(leaf&8))s3=v;
        else {v=tensorcore_f32x4_add(s3,v);if(!(leaf&16))s4=v;
        else result=tensorcore_f32x4_add(s4,v);}}}}
    }
    /* The row realization admits finite-input overflow recovery, not a
     * different failure policy for wide populations. Rescan exceptional dots
     * through the same source-ordered F64 oracle before publishing. */
    float values[] = {result.x, result.y, result.z, result.w};
    for (unsigned int coordinate = 0u; coordinate < 4u; ++coordinate) {
        unsigned long long row = rowbase + group + (coordinate >> 1u) * 8ull;
        unsigned int column = columnbase + t * 2u + (coordinate & 1u);
        if (row < rows && column < pop && !isfinite(values[coordinate])) {
            unsigned long long source = ordered ? offset + column : order[offset + column] / topk;
            values[coordinate] = qtype_q8_dot_recover_f64(weights + row * rb,
                input + source * blocks * YVEX_CUDA_Q8_K_BYTES, blocks,
                qtype == YVEX_GGUF_QTYPE_IQ2_XXS ? 66ull : 84ull, qtype, status);
        }
    }
    return make_float4(values[0], values[1], values[2], values[3]);
}
/* Compact the real eligible bucket populations into independent eight-column
 * tiles. The immutable worklist remains authoritative; a conservative launch
 * bound may contain padding, but never duplicates a selected pair. Each warp
 * owns one output tile rather than serially visiting an entire expert bucket. */
static __device__ unsigned long long tensorcore_expert_column_tile(
    unsigned long long tile, const unsigned long long *populations,
    unsigned long long buckets, unsigned long long minimum,
    unsigned int *column_base, int *status)
{
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long cursor = 0ull;
    *column_base = 0u;
    /* The launch includes conservative padding. Resolve its compact ordinal
     * cooperatively instead of making lane zero rescan every expert bucket
     * for every output-row tile. Prefixes retain the original bucket order. */
    for (unsigned long long first = 0ull; first < buckets; first += 32ull) {
        unsigned long long index = first + lane;
        unsigned long long population = index < buckets ? populations[index] : 0ull;
        if (__any_sync(0xffffffffu, population > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH)) {
            if (!lane) atomicCAS(status, 0, 2);
            return buckets;
        }
        unsigned int extent = population >= minimum ?
            (unsigned int)((population + 7ull) / 8ull) : 0u;
        unsigned int inclusive = extent;
        for (unsigned int offset = 1u; offset < 32u; offset <<= 1u) {
            unsigned int previous = __shfl_up_sync(0xffffffffu, inclusive, offset);
            if (lane >= offset) inclusive += previous;
        }
        unsigned long long begin = cursor + inclusive - extent;
        unsigned int owners = __ballot_sync(0xffffffffu,
            tile >= begin && tile - begin < extent);
        if (owners) {
            unsigned int owner = (unsigned int)__ffs(owners) - 1u;
            *column_base = (unsigned int)(tile - __shfl_sync(0xffffffffu, begin, owner)) * 8u;
            return __shfl_sync(0xffffffffu, index, owner);
        }
        cursor += __shfl_sync(0xffffffffu, inclusive, 31);
    }
    return buckets;
}

extern "C" __global__ void yvex_moe_grouped_up_tensorcore(
    const unsigned char *gate, unsigned long long gate_row_bytes,
    unsigned long long gate_expert_bytes, unsigned int gate_qtype,
    const unsigned char *up, unsigned long long up_row_bytes,
    unsigned long long up_expert_bytes, unsigned int up_qtype,
    const unsigned long long *selected, const float *weights,
    const unsigned long long *order, const unsigned long long *expert_ids,
    const unsigned long long *bucket_offsets,
    const unsigned long long *bucket_populations,
    const yvex_expert_worklist_observation *summary,
    unsigned long long pair_count, unsigned long long topk,
    unsigned long long expert_count, unsigned long long tensor_core_minimum,
    const unsigned char *input, unsigned long long input_width,
    unsigned long long intermediate_width, double limit,
    float *intermediate, int *status)
{
    /* IQ2 fragment lanes use different codebook entries. Stage the canonical
     * table once rather than serializing divergent constant-memory reads. */
    __shared__ unsigned int grid_table[512];
    if (gate_qtype == YVEX_GGUF_QTYPE_IQ2_XXS || up_qtype == YVEX_GGUF_QTYPE_IQ2_XXS)
        tensorcore_iq2_grid(grid_table);
    __syncthreads();
    unsigned int warp = threadIdx.x >> 5u, lane = threadIdx.x & 31u;
    unsigned long long tiles = (intermediate_width + 15ull) / 16ull;
    unsigned long long task = (unsigned long long)blockIdx.x * 4ull + warp;
    unsigned long long row_base = tiles ? (task % tiles) * 16ull : intermediate_width;
    if (!status || *status || warp >= 4u || !summary || !expert_ids ||
        !bucket_offsets || !bucket_populations) return;
    if (!tiles || !tensor_core_minimum || summary->bucket_count > expert_count ||
        summary->bucket_count > pair_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned int column_base = 0u;
    unsigned long long bucket = tensorcore_expert_column_tile(task / tiles,
        bucket_populations, summary->bucket_count, tensor_core_minimum, &column_base, status);
    if (bucket >= summary->bucket_count) return;
    unsigned long long offset = bucket_offsets[bucket];
    unsigned long long population = bucket_populations[bucket];
    unsigned long long expert = expert_ids[bucket];
    unsigned long long input_blocks = input_width / YVEX_CUDA_Q8_K_BLOCK;
    if (!gate || !up || !selected || !weights || !order || !input || !intermediate ||
        !topk || !input_blocks || input_blocks > 32ull || !intermediate_width ||
        !tensor_core_minimum ||
        (gate_qtype != YVEX_GGUF_QTYPE_IQ2_XXS && gate_qtype != YVEX_GGUF_QTYPE_Q2_K) ||
        (up_qtype != YVEX_GGUF_QTYPE_IQ2_XXS && up_qtype != YVEX_GGUF_QTYPE_Q2_K)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    if (population < tensor_core_minimum) return;
    if (population > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH || offset > pair_count ||
        population > pair_count - offset || expert >= expert_count ||
        !isfinite(limit) || limit <= 0.0) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    if (!lane) for (unsigned long long column = column_base;
                   column < population && column < column_base + 8ull; ++column) {
        unsigned long long source_pair = order[offset + column];
        if (source_pair >= pair_count || selected[source_pair] != expert)
            atomicCAS(status, 0, 2);
    }
    __syncwarp();
    if (*status) return;
    if (!tensorcore_row_geometry(gate_qtype, input_width, gate_row_bytes) ||
        !tensorcore_row_geometry(up_qtype, input_width, up_row_bytes) ||
        gate_expert_bytes != intermediate_width * gate_row_bytes ||
        up_expert_bytes != intermediate_width * up_row_bytes) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    {
        float4 gate_values = tensorcore_expert_register_dot(gate + expert * gate_expert_bytes,
            gate_row_bytes, intermediate_width, row_base, input, (unsigned int)input_blocks,
            order, offset, (unsigned int)population, (unsigned int)topk,
            column_base, gate_qtype, 0, status, grid_table);
        float4 up_values = tensorcore_expert_register_dot(up + expert * up_expert_bytes,
            up_row_bytes, intermediate_width, row_base, input, (unsigned int)input_blocks,
            order, offset, (unsigned int)population, (unsigned int)topk,
            column_base, up_qtype, 0, status, grid_table);
        const float gates[] = {gate_values.x, gate_values.y, gate_values.z, gate_values.w};
        const float ups[] = {up_values.x, up_values.y, up_values.z, up_values.w};
        for (unsigned int coordinate = 0u; coordinate < 4u; ++coordinate) {
            unsigned long long row = row_base + (lane >> 2u) + (coordinate >> 1u) * 8ull;
            unsigned int column = column_base + (lane & 3u) * 2u + (coordinate & 1u);
            if (row < intermediate_width && column < population && !*status) {
                unsigned long long ordered_pair = offset + column, source_pair = order[ordered_pair];
                float value;
                if (!cuda_clamped_swiglu_bf16_value(gates[coordinate], ups[coordinate],
                        limit, weights[source_pair], &value)) atomicCAS(status, 0, 1);
                else intermediate[ordered_pair * intermediate_width + row] = value;
            }
        }
    }
}

extern "C" __global__ void yvex_moe_grouped_down_tensorcore(
    const unsigned char *down, unsigned long long row_bytes,
    unsigned long long expert_bytes, unsigned int qtype,
    const unsigned long long *selected, const unsigned long long *order,
    const unsigned long long *expert_ids,
    const unsigned long long *bucket_offsets,
    const unsigned long long *bucket_populations,
    yvex_expert_worklist_observation *summary,
    unsigned long long pair_count, unsigned long long topk,
    unsigned long long expert_count, unsigned long long tensor_core_minimum,
    const unsigned char *intermediate, unsigned long long intermediate_width,
    unsigned long long hidden, float *pair_outputs, int *status)
{
    unsigned int warp = threadIdx.x >> 5u, lane = threadIdx.x & 31u;
    unsigned long long tiles = (hidden + 15ull) / 16ull;
    unsigned long long task = (unsigned long long)blockIdx.x * 4ull + warp;
    unsigned long long row_base = tiles ? (task % tiles) * 16ull : hidden;
    if (!status || *status || warp >= 4u || !summary || !expert_ids ||
        !bucket_offsets || !bucket_populations) return;
    if (!tiles || !tensor_core_minimum || summary->bucket_count > expert_count ||
        summary->bucket_count > pair_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned int column_base = 0u;
    unsigned long long bucket = tensorcore_expert_column_tile(task / tiles,
        bucket_populations, summary->bucket_count, tensor_core_minimum, &column_base, status);
    if (bucket >= summary->bucket_count) return;
    unsigned long long offset = bucket_offsets[bucket];
    unsigned long long population = bucket_populations[bucket];
    unsigned long long expert = expert_ids[bucket];
    unsigned long long input_blocks = intermediate_width / YVEX_CUDA_Q8_K_BLOCK;
    if (!down || !selected || !order || !intermediate || !pair_outputs || !topk ||
        !input_blocks || input_blocks > 32ull || !hidden || !tensor_core_minimum ||
        qtype != YVEX_GGUF_QTYPE_Q2_K) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    if (population < tensor_core_minimum) return;
    if (population > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH || offset > pair_count ||
        population > pair_count - offset || expert >= expert_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    if (!lane) for (unsigned long long column = column_base;
                   column < population && column < column_base + 8ull; ++column) {
        unsigned long long source_pair = order[offset + column];
        if (source_pair >= pair_count || selected[source_pair] != expert)
            atomicCAS(status, 0, 2);
    }
    __syncwarp();
    if (*status) return;
    if (!tensorcore_row_geometry(qtype, intermediate_width, row_bytes) ||
        expert_bytes != hidden * row_bytes) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    {
        float4 dot = tensorcore_expert_register_dot(down + expert * expert_bytes,
            row_bytes, hidden, row_base, intermediate, (unsigned int)input_blocks,
            order, offset, (unsigned int)population, (unsigned int)topk, column_base, qtype, 1, status);
        const float values[] = {dot.x, dot.y, dot.z, dot.w};
        for (unsigned int coordinate = 0u; coordinate < 4u; ++coordinate) {
            unsigned long long row = row_base + (lane >> 2u) + (coordinate >> 1u) * 8ull;
            unsigned int column = column_base + (lane & 3u) * 2u + (coordinate & 1u);
            if (row < hidden && column < population && !*status) {
                float value = float_to_bf16_rne(values[coordinate]);
                if (!isfinite(value)) atomicCAS(status, 0, 1);
                else pair_outputs[order[offset + column] * hidden + row] = value;
            }
        }
    }
    if (!lane && !row_base && !*status)
        atomicAdd(&summary->matrix_tile_executed_pairs,
                  population - column_base < 8ull ? population - column_base : 8ull);
}
