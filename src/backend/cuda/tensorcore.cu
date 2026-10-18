/*
 * Execute admitted low-bit rows by Q8_K activations through native integer Tensor Cores.
 *
 * Encoded scales remain outside MMA. Each 16-element segment is expanded directly into an
 * integer fragment, so Q8_0, Q2_K, IQ2_XXS, and MXFP4 keep their canonical block semantics
 * without materializing a dense dequantized matrix.
 */
#include "src/backend/cuda/kernel_primitives.h"
#include <yvex/internal/execution_observation.h>
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
/* Q2 minimum correction is another exact matrix product, not a per-output
 * scalar reduction. Split each signed I16 activation subtotal into unsigned
 * low and signed high bytes: s = lo + 256*hi. Two integer MMAs cover sixteen
 * real terms (the other K lanes are zero). Even arbitrary encoded I16 values
 * satisfy |sum(min*s)| <= 16*15*32768, so all intermediates fit I32. */
static __device__ void tensorcore_q2_minimum(int result[4],
    const unsigned char *weights, unsigned long long row_bytes,
    unsigned long long rows, unsigned long long row_base,
    const unsigned char *input, unsigned blocks, unsigned block,
    const unsigned long long *order, unsigned long long offset,
    unsigned population, unsigned topk, unsigned column_base, int ordered)
{
    unsigned group = (threadIdx.x & 31u) >> 2u, lane = threadIdx.x & 3u;
    int a[4] = {}, low[2] = {}, high[2] = {}, upper[4] = {};
#pragma unroll
    for (unsigned j = 0u; j < 2u; ++j) {
        unsigned long long row = row_base + group + j * 8ull;
        if (row < rows)
            a[j] = (int)((qtype_load_u32(weights + row * row_bytes +
                block * 84ull + lane * 4u) >> 4u) & 0x0f0f0f0fu);
    }
    unsigned column = column_base + group;
    if (column < population) {
        unsigned long long source = ordered ? offset + column : order[offset + column] / topk;
        const unsigned char *sums = input + (source * blocks + block) *
            YVEX_CUDA_Q8_K_BYTES + 260ull + lane * 8u;
        unsigned first = qtype_load_u32(sums), second = qtype_load_u32(sums + 4u);
        low[0] = (int)__byte_perm(first, second, 0x6420u);
        high[0] = (int)__byte_perm(first, second, 0x7531u);
    }
    asm volatile("mma.sync.aligned.m16n8k32.row.col.s32.s8.u8.s32 "
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(result[0]), "+r"(result[1]), "+r"(result[2]), "+r"(result[3])
        : "r"(a[0]), "r"(a[1]), "r"(a[2]), "r"(a[3]), "r"(low[0]), "r"(low[1]));
    tensorcore_integer_mma(upper, a, high);
#pragma unroll
    for (unsigned j = 0u; j < 4u; ++j) result[j] += 256 * upper[j];
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

/* A CTA stages one encoded K256 cohort (64 weight rows, 32 Q8_K inputs).
 * The four row tiles share activations; the two column tiles share weights.
 * No repacked representation survives the CTA. Integer products and the
 * original eight source-ordered F32 MXFP4 block publications are unchanged. */
extern "C" __global__ void yvex_mxfp4_tensorcore_shared_rows(
    const unsigned char *encoded, unsigned long long row_bytes, unsigned long long row_width,
    unsigned long long start_row, unsigned long long row_count, unsigned long long input_rows,
    unsigned long long group_count, unsigned long long group_rows, unsigned qtype,
    const unsigned char *activation, const float *additive, float *output, int output_bf16, int *status)
{
    __shared__ unsigned weight_words[64 * 34], activation_words[32 * 73];
    __shared__ int active;
    if (!status) return;
    if (!threadIdx.x) active = *status == 0;
    __syncthreads();
    if (!active) return;
    if (!encoded || !activation || !output || !row_count || !input_rows || !group_count || !group_rows ||
        group_count > ~0ull / group_rows || row_count != group_count * group_rows ||
        blockDim.x != 256u || input_rows > ~0ull - 31ull ||
        (group_count > 1 && group_rows % 64) || qtype != YVEX_GGUF_QTYPE_MXFP4 ||
        !tensorcore_row_geometry(qtype, row_width, row_bytes)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    const unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    const unsigned long long input_groups = (input_rows + 31) / 32;
    const unsigned long long row_base = (blockIdx.x / input_groups) * 64;
    const unsigned long long input_base = (blockIdx.x % input_groups) * 32;
    if (row_base >= row_count) return;
    const unsigned long long row_group = row_base / group_rows, blocks = row_width / 256;
    const unsigned char *w = (const unsigned char *)weight_words;
    const unsigned char *x = (const unsigned char *)activation_words;
    float totals[2][4] = {};
    for (unsigned long long block = 0; block < blocks; block++) {
        for (unsigned i = threadIdx.x; i < 64 * 34; i += 256) {
            unsigned long long row = row_base + i / 34;
            weight_words[i] = row < row_count ? qtype_load_u32(
                encoded + (start_row + row) * row_bytes + block * 136 + (i % 34) * 4) : 0;
        }
        for (unsigned i = threadIdx.x; i < 32 * 73; i += 256) {
            unsigned long long column = input_base + i / 73;
            activation_words[i] = column < input_rows ? qtype_load_u32(activation +
                ((column * group_count + row_group) * blocks + block) * 292 + (i % 73) * 4) : 0;
        }
        __syncthreads();
#pragma unroll
        for (unsigned segment = 0; segment < 8; segment++) {
            int a[4];
#pragma unroll
            for (unsigned j = 0; j < 4; j++) {
                unsigned r = (warp / 2) * 16 + (lane >> 2) + (j & 1) * 8;
                unsigned first = (lane & 3) * 4 + (j >> 1) * 16;
                /* Canonical 17-byte blocks cross shared-word boundaries.
                 * Read aligned words and assemble the exact quartet instead
                 * of four byte-addressed shared loads. The last quartet ends
                 * at byte 135 of its 136-byte cohort; no next row is read. */
                unsigned offset = r * 136 + segment * 17 + 1 + (first & 15u);
                unsigned index = offset >> 2, shift = (offset & 3u) * 8u;
                unsigned packed = __funnelshift_r(weight_words[index],
                    weight_words[index + (shift != 0)], shift);
                a[j] = mxfp4_i8x4(packed, first >= 16u);
            }
#pragma unroll
            for (unsigned tile = 0; tile < 2; tile++) {
                int b[2], product[4] = {};
                unsigned column = (warp % 2) * 16 + tile * 8 + (lane >> 2);
#pragma unroll
                for (unsigned j = 0; j < 2; j++)
                    b[j] = (int)qtype_load_u32(x + column * 292 + 4 + segment * 32 + (lane & 3) * 4 + j * 16);
                tensorcore_integer_mma(product, a, b);
#pragma unroll
                for (unsigned j = 0; j < 4; j++) {
                    unsigned r = (warp / 2) * 16 + (lane >> 2) + (j >> 1) * 8;
                    unsigned c = (warp % 2) * 16 + tile * 8 + (lane & 3) * 2 + (j & 1);
                    float scale = __uint_as_float(activation_words[c * 73]);
                    float term = tensorcore_scaled_group(w + r * 136, segment * 32,
                        YVEX_GGUF_QTYPE_MXFP4, scale, product[j], 0);
                    totals[tile][j] = __fadd_rn(totals[tile][j], term);
                }
            }
        }
        __syncthreads();
    }
#pragma unroll
    for (unsigned tile = 0; tile < 2; tile++) {
#pragma unroll
        for (unsigned j = 0; j < 4; j++) {
            unsigned long long row = row_base + (warp / 2) * 16 + (lane >> 2) + (j >> 1) * 8;
            unsigned long long column = input_base + (warp % 2) * 16 + tile * 8 + (lane & 3) * 2 + (j & 1);
            if (row < row_count && column < input_rows) {
                unsigned long long i = column * row_count + row;
                float value = additive ? __fadd_rn(totals[tile][j], additive[i]) : totals[tile][j];
                if (!isfinite(value)) atomicCAS(status, 0, 1);
                else output[i] = output_bf16 ? float_to_bf16_rne(value) : value;
            }
        }
    }
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
    unsigned qtype, int ordered, int *status, const unsigned int *grid_table = nullptr,
    int operand_failure = 1);

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
            (qtype == YVEX_GGUF_QTYPE_Q2_K ? 84ull :
             qtype == YVEX_GGUF_QTYPE_Q8_0 ? 272ull : 136ull) ||
        !rows || rows > 131072ull || start > ~0ull - rows ||
        (start + rows) > ~0ull / row_bytes || !inputs ||
        inputs > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH ||
        (qtype != YVEX_GGUF_QTYPE_MXFP4 && qtype != YVEX_GGUF_QTYPE_Q2_K &&
         qtype != YVEX_GGUF_QTYPE_Q8_0) ||
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
                        (block * 8u + part) * (qtype == YVEX_GGUF_QTYPE_Q8_0 ? 34u : 17u) +
                        (qtype == YVEX_GGUF_QTYPE_Q8_0 ? 2u + (j >> 1u) * 16u : 1u) +
                        (lane & 3u) * 4u;
                    a[j] = row >= rows ? 0 : qtype == YVEX_GGUF_QTYPE_Q8_0
                        ? (int)qtype_load_u32(w) : mxfp4_i8x4(qtype_load_u32(w), j >> 1u);
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
                        float ws = qtype == YVEX_GGUF_QTYPE_Q8_0
                            ? f16_bits_to_float(qtype_load_u16(weights + (start + row) * row_bytes +
                                (block * 8u + part) * 34u))
                            : e8m0_bits_to_float(weights[(start + row) * row_bytes +
                                (block * 8u + part) * 17u]) * 0.5f;
                        float as = __uint_as_float(qtype_load_u32(activation +
                            (col * blocks + block) * 292ull));
                        values[j] = fmaf(ws * as, (float)products[j], values[j]);
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
                blocks, qtype == YVEX_GGUF_QTYPE_Q2_K ? 84ull :
                    qtype == YVEX_GGUF_QTYPE_Q8_0 ? 272ull : 136ull, qtype, status);
            if (additive) value = __fadd_rn(value, additive[col * output_stride + row]);
            if (output_bf16) value = float_to_bf16_rne(value);
            if (!isfinite(value)) atomicCAS(status, 0, 1);
            else output[col * output_stride + row] = value;
        }
    }
}
/* A Q2 CTA owns 64 rows by eight real expert columns. Stage the next real
 * block while computing the current one; share Q8 input across four row warps.
 * Storage, integer products, the 32-leaf F32 tree and recovery are unchanged.
 * Shared storage is CTA-local: no persistent preparation or model duplicate. */

static __device__ void q2_pipeline_copy(unsigned *destination,
                                       const unsigned char *source, bool present)
{
    // Canonical package offsets may be unaligned. That case uses the existing
    // bounded byte decoder; it must never issue an unaligned asynchronous copy.
#if __CUDA_ARCH__ >= 800
    if (present && !((unsigned long long)source & 3ull)) {
        unsigned address = (unsigned)__cvta_generic_to_shared(destination);
        asm volatile("cp.async.ca.shared.global [%0], [%1], 4;"
                     :: "r"(address), "l"(source) : "memory");
        return;
    }
#endif
    *destination = present ? qtype_load_u32(source) : 0u;
}

template<unsigned COLS = 8u, unsigned AREA = 512u>
static __device__ void q2_pipeline_stage(unsigned *w, unsigned *x,
    const unsigned char *weights, unsigned long long row_bytes,
    unsigned long long rows, unsigned long long first_row,
    const unsigned char *input, unsigned blocks, unsigned block,
    const unsigned long long *order, unsigned long long offset,
    unsigned population, unsigned topk, unsigned column_base, int ordered)
{
    for (unsigned i = threadIdx.x; i < (AREA / COLS) * 21u; i += blockDim.x) {
        unsigned row = i / 21u, word = i % 21u;
        bool present = first_row + row < rows;
        const unsigned char *address = present ? weights +
            (first_row + row) * row_bytes + block * 84ull + word * 4u : weights;
        q2_pipeline_copy(w + i, address, present);
    }
    for (unsigned i = threadIdx.x; i < COLS * 73u; i += blockDim.x) {
        unsigned column = column_base + i / 73u, word = i % 73u;
        bool present = column < population;
        unsigned long long index = present ?
            (ordered ? offset + column : order[offset + column] / topk) : 0ull;
        const unsigned char *address = present ? input +
            (index * blocks + block) * 292ull + word * 4u : input;
        q2_pipeline_copy(x + i, address, present);
    }
#if __CUDA_ARCH__ >= 800
    asm volatile("cp.async.commit_group;" ::: "memory");
#endif
}

static __device__ float4 tensorcore_expert_shared_dot(
    const unsigned char *weights, unsigned long long row_bytes,
    unsigned long long rows, unsigned long long row_base,
    const unsigned char *input, unsigned blocks, const unsigned long long *order,
    unsigned long long offset, unsigned population, unsigned topk,
    unsigned column_base, int ordered, int *status)
{
    __shared__ unsigned staged_weights[2][64 * 21];
    __shared__ unsigned staged_input[2][8 * 73];
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    unsigned group = lane >> 2u, thread = lane & 3u;
    unsigned long long first_row = row_base - warp * 16u;
    float4 s0 = {}, s1 = {}, s2 = {}, s3 = {}, s4 = {}, result = {};
    unsigned slot = 0u;
    q2_pipeline_stage(staged_weights[0], staged_input[0], weights, row_bytes,
        rows, first_row, input, blocks, 0u, order, offset, population, topk,
        column_base, ordered);
    for (unsigned leaf = 0u; leaf < 32u; ++leaf) {
        unsigned block = __brev(leaf) >> 27u;
        float4 value = {};
        if (block < blocks) {
#if __CUDA_ARCH__ >= 800
            asm volatile("cp.async.wait_group 0;" ::: "memory");
#endif
            __syncthreads();
            unsigned next = leaf + 1u;
            while (next < 32u && (__brev(next) >> 27u) >= blocks) ++next;
            if (next < 32u)
                q2_pipeline_stage(staged_weights[slot ^ 1u], staged_input[slot ^ 1u],
                    weights, row_bytes, rows, first_row, input, blocks,
                    __brev(next) >> 27u, order, offset, population, topk,
                    column_base, ordered);
            const unsigned *w = staged_weights[slot], *x = staged_input[slot];
            int product[4] = {};
#pragma unroll
            for (unsigned segment = 0u; segment < 256u; segment += 32u) {
                int a[4], b[2];
#pragma unroll
                for (unsigned j = 0u; j < 4u; ++j) {
                    unsigned row = warp * 16u + group + (j & 1u) * 8u;
                    unsigned first = segment + thread * 4u + (j >> 1u) * 16u;
                    unsigned bits = w[row * 21u + 4u + first / 128u * 8u + first % 32u / 4u];
                    unsigned scale = (w[row * 21u + first / 64u] >>
                        (first / 16u % 4u * 8u)) & 15u;
                    a[j] = (int)(((bits >> (first % 128u / 32u * 2u)) & 0x03030303u) * scale);
                }
                for (unsigned j = 0u; j < 2u; ++j)
                    b[j] = (int)x[group * 73u + 1u + segment / 4u + thread + j * 4u];
                tensorcore_integer_mma(product, a, b);
            }
            int minimum[4] = {};
            tensorcore_q2_minimum(minimum, (const unsigned char *)w, 84ull, 64ull,
                warp * 16u, (const unsigned char *)x, 1u, 0u, nullptr, 0ull, 8u, 1u, 0u, 1);
            float values[4] = {};
            for (unsigned j = 0u; j < 4u; ++j) {
                unsigned row = warp * 16u + group + (j >> 1u) * 8u;
                unsigned column = thread * 2u + (j & 1u);
                if (first_row + row < rows && column_base + column < population) {
                    float scale = __uint_as_float(x[column * 73u]);
                    unsigned scales = w[row * 21u + 20u];
                    values[j] = scale * f16_bits_to_float(scales & 65535u) * (float)product[j] -
                        scale * f16_bits_to_float(scales >> 16u) * (float)minimum[j];
                }
            }
            value = make_float4(values[0], values[1], values[2], values[3]);
            __syncthreads();
            slot ^= 1u;
        }
        if (!(leaf & 1u)) s0 = value;
        else { value = tensorcore_f32x4_add(s0, value); if (!(leaf & 2u)) s1 = value;
        else { value = tensorcore_f32x4_add(s1, value); if (!(leaf & 4u)) s2 = value;
        else { value = tensorcore_f32x4_add(s2, value); if (!(leaf & 8u)) s3 = value;
        else { value = tensorcore_f32x4_add(s3, value); if (!(leaf & 16u)) s4 = value;
        else result = tensorcore_f32x4_add(s4, value); }}}}
    }
    float values[] = {result.x, result.y, result.z, result.w};
    for (unsigned j = 0u; j < 4u; ++j) {
        unsigned long long row = row_base + group + (j >> 1u) * 8u;
        unsigned column = column_base + thread * 2u + (j & 1u);
        if (row < rows && column < population && !isfinite(values[j])) {
            unsigned long long index = ordered ? offset + column : order[offset + column] / topk;
            values[j] = qtype_q8_dot_recover_f64(weights + row * row_bytes,
                input + index * blocks * 292ull, blocks, 84ull, YVEX_GGUF_QTYPE_Q2_K,
                status, ordered ? YVEX_MOE_DEVICE_DOWN_OPERAND : YVEX_MOE_DEVICE_GATE_OPERAND);
        }
    }
    return make_float4(values[0], values[1], values[2], values[3]);
}

/* Paired Q2 projection: common activation stage and block traversal.
 * Both independent dots keep their original 32-leaf F32 trees.
 */
struct tensorcore_pair_result { float4 gate, up; };
struct tensorcore_pair_tree { float4 s0,s1,s2,s3,s4,result; };

static __device__ __forceinline__ void tensorcore_pair_push(tensorcore_pair_tree &tree,float4 value,unsigned leaf)
{
    if (!(leaf & 1u)) tree.s0=value;
    else { value=tensorcore_f32x4_add(tree.s0,value); if (!(leaf & 2u)) tree.s1=value;
    else { value=tensorcore_f32x4_add(tree.s1,value); if (!(leaf & 4u)) tree.s2=value;
    else { value=tensorcore_f32x4_add(tree.s2,value); if (!(leaf & 8u)) tree.s3=value;
    else { value=tensorcore_f32x4_add(tree.s3,value); if (!(leaf & 16u)) tree.s4=value;
    else tree.result=tensorcore_f32x4_add(tree.s4,value); }}}}
}

template<unsigned COLS>
static __device__ void tensorcore_pair_stage(unsigned *g,unsigned *u,unsigned *x,
    const unsigned char *gate,const unsigned char *up,unsigned long long rb,
    unsigned long long rows,unsigned long long first_row,
    const unsigned char *input,unsigned blocks,unsigned block,const unsigned long long *order,
    unsigned long long offset,unsigned population,unsigned topk,unsigned column_base,int ordered)
{
    q2_pipeline_stage<COLS,1024u>(g,x,gate,rb,rows,first_row,input,blocks,block,
        order,offset,population,topk,column_base,ordered);
    for(unsigned i=threadIdx.x;i<(1024u/COLS)*21u;i+=blockDim.x) {
        unsigned row=i/21u,word=i%21u;
        bool present=first_row+row<rows;
        const unsigned char *address=present?up+(first_row+row)*rb+block*84ull+word*4u:up;
        q2_pipeline_copy(u+i,address,present);
    }
#if __CUDA_ARCH__ >= 800
    asm volatile("cp.async.commit_group;" ::: "memory");
#endif
}

template<unsigned COLS>
static __device__ __forceinline__ tensorcore_pair_result tensorcore_pair_dot(const unsigned char *gate,
    const unsigned char *up,unsigned long long rb,unsigned long long rows,
    unsigned long long row_base,const unsigned char *input,
    unsigned blocks,const unsigned long long *order,unsigned long long offset,unsigned population,unsigned topk,
    unsigned column_base,int ordered,int *status)
{
    __shared__ unsigned gate_stage[2][(1024/COLS)*21];
    __shared__ unsigned up_stage[2][(1024/COLS)*21];
    __shared__ unsigned input_stage[2][COLS*73];
    unsigned lane=threadIdx.x&31u,warp=threadIdx.x>>5u;
    unsigned group=lane>>2u,thread=lane&3u;
    unsigned warp_row=warp/(COLS/8u)*16u,warp_column=warp%(COLS/8u)*8u;
    unsigned long long first_row=row_base-warp_row;
    tensorcore_pair_tree trees[2]={};
    unsigned slot=0u;
    tensorcore_pair_stage<COLS>(gate_stage[0],up_stage[0],input_stage[0],gate,up,rb,
        rows,first_row,input,blocks,0u,order,offset,population,topk,column_base,ordered);
    for(unsigned leaf=0;leaf<32;leaf++) {
        unsigned block=__brev(leaf)>>27u;
        float4 values[2]={};
        if(block<blocks) {
#if __CUDA_ARCH__ >= 800
            asm volatile("cp.async.wait_group 0;" ::: "memory");
#endif
            __syncthreads();
            unsigned next=leaf+1;
            while(next<32&&(__brev(next)>>27u)>=blocks)next++;
            if(next<32)tensorcore_pair_stage<COLS>(gate_stage[slot^1u],up_stage[slot^1u],
                input_stage[slot^1u],gate,up,rb,rows,first_row,input,blocks,
                __brev(next)>>27u,order,offset,population,topk,column_base,ordered);
            const unsigned *x=input_stage[slot];
            int products[2][4]={};
#pragma unroll
            for(unsigned segment=0;segment<256;segment+=32) {
                int b[2];
#pragma unroll
                for(unsigned j=0;j<2;j++)
                    b[j]=(int)x[(warp_column+group)*73u+1u+segment/4u+thread+j*4u];
#pragma unroll
                for(unsigned which=0;which<2;which++) {
                    const unsigned *w=which?up_stage[slot]:gate_stage[slot];
                    int a[4];
#pragma unroll
                    for(unsigned j=0;j<4;j++) {
                        unsigned row=warp_row+group+(j&1u)*8u;
                        unsigned first=segment+thread*4u+(j>>1u)*16u;
                        unsigned bits=w[row*21u+4u+first/128u*8u+first%32u/4u];
                        unsigned scale=(w[row*21u+first/64u]>>(first/16u%4u*8u))&15u;
                        a[j]=(int)(((bits>>(first%128u/32u*2u))&0x03030303u)*scale);
                    }
                    tensorcore_integer_mma(products[which],a,b);
                }
            }
#pragma unroll
            for(unsigned which=0;which<2;which++) {
                const unsigned *w=which?up_stage[slot]:gate_stage[slot];
                int minimum[4]={};
                tensorcore_q2_minimum(minimum,(const unsigned char *)w,84ull,1024ull/COLS,
                    warp_row,(const unsigned char *)(x+warp_column*73u),1u,0u,
                    nullptr,0ull,8u,1u,0u,1);
                float scalars[4]={};
#pragma unroll
                for(unsigned j=0;j<4;j++) {
                    unsigned row=warp_row+group+(j>>1u)*8u;
                    unsigned column=warp_column+thread*2u+(j&1u);
                    if(first_row+row<rows&&column_base+column<population) {
                        float scale=__uint_as_float(x[column*73u]);
                        unsigned scales=w[row*21u+20u];
                        scalars[j]=scale*f16_bits_to_float(scales&65535u)*(float)products[which][j]-
                            scale*f16_bits_to_float(scales>>16u)*(float)minimum[j];
                    }
                }
                values[which]=make_float4(scalars[0],scalars[1],scalars[2],scalars[3]);
            }
            __syncthreads();slot^=1u;
        }
#pragma unroll
        for(unsigned which=0;which<2;which++)tensorcore_pair_push(trees[which],values[which],leaf);
    }
    tensorcore_pair_result result={trees[0].result,trees[1].result};
#pragma unroll
    for(unsigned which=0;which<2;which++) {
        float4 v=which?result.up:result.gate;
        float values[]={v.x,v.y,v.z,v.w};
#pragma unroll
        for(unsigned j=0;j<4;j++) {
            unsigned long long row=row_base+group+(j>>1u)*8u;
            unsigned column=column_base+warp_column+thread*2u+(j&1u);
            if(row<rows&&column<population&&!isfinite(values[j])) {
                unsigned long long index=ordered?offset+column:order[offset+column]/topk;
                const unsigned char *w=which?up:gate;
                values[j]=qtype_q8_dot_recover_f64(w+row*rb,input+index*blocks*292ull,
                    blocks,84ull,YVEX_GGUF_QTYPE_Q2_K,status,
                    which ? YVEX_MOE_DEVICE_UP_OPERAND : YVEX_MOE_DEVICE_GATE_OPERAND);
            }
        }
        v=make_float4(values[0],values[1],values[2],values[3]);
        if(which)result.up=v;else result.gate=v;
    }
    return result;
}

/* An IQ2 block is 66 bytes. Stage its enclosing 68-byte aligned interval,
 * keeping the source's halfword displacement rather than reading unaligned
 * encoded words throughout MMA. Even block populations keep the final copy
 * within its row. This is CTA-local preparation, not another weight copy. */
static __device__ void tensorcore_iq2_pair_stage(unsigned *g, unsigned *u, unsigned *x,
    const unsigned char *gate, const unsigned char *up, unsigned long long rb,
    unsigned long long rows, unsigned long long first_row,
    const unsigned char *input, unsigned blocks, unsigned block,
    const unsigned long long *order, unsigned long long offset, unsigned population,
    unsigned topk, unsigned column_base, int ordered)
{
    unsigned aligned = (block * 66u) & ~3u;
    for (unsigned i = threadIdx.x; i < 64u * 17u; i += blockDim.x) {
        unsigned row = i / 17u, word = i % 17u;
        bool present = first_row + row < rows;
        const unsigned char *ga = present ? gate + (first_row + row) * rb + aligned + word * 4u : gate;
        const unsigned char *ua = present ? up + (first_row + row) * rb + aligned + word * 4u : up;
        q2_pipeline_copy(g + i, ga, present);
        q2_pipeline_copy(u + i, ua, present);
    }
    for (unsigned i = threadIdx.x; i < 16u * 73u; i += blockDim.x) {
        unsigned column = column_base + i / 73u, word = i % 73u;
        bool present = column < population;
        unsigned long long source = present ? (ordered ? offset + column : order[offset + column] / topk) : 0ull;
        const unsigned char *address = present ? input + (source * blocks + block) * 292ull + word * 4u : input;
        q2_pipeline_copy(x + i, address, present);
    }
#if __CUDA_ARCH__ >= 800
    asm volatile("cp.async.commit_group;" ::: "memory");
#endif
}

static __device__ unsigned tensorcore_iq2_word(const unsigned *row, unsigned offset)
{
    unsigned word = offset >> 2u, shift = (offset & 3u) * 8u;
    return __funnelshift_r(row[word], row[word + (shift != 0u)], shift);
}

static __device__ int tensorcore_iq2_digit4(const unsigned *row, unsigned displacement,
    unsigned first, const unsigned *grid_table)
{
    unsigned group = first / 32u, subgroup = first % 32u / 8u;
    unsigned grids = tensorcore_iq2_word(row, displacement + 2u + group * 8u);
    unsigned ss = tensorcore_iq2_word(row, displacement + 6u + group * 8u);
    unsigned index = (grids >> (subgroup * 8u)) & 255u;
    unsigned signs = iq2_xxs_signs((ss >> (subgroup * 7u)) & 127u);
    unsigned part = (first & 4u) >> 2u, packed = grid_table[index * 2u + part];
    unsigned bits = (signs >> (part * 4u)) & 15u;
    unsigned mask = ((bits * 0x00204081u) & 0x01010101u) * 255u;
    return (int)__vsub4(packed ^ mask, mask);
}

static __device__ __forceinline__ tensorcore_pair_result tensorcore_iq2_pair_dot(
    const unsigned char *gate, const unsigned char *up, unsigned long long rb,
    unsigned long long rows, unsigned long long row_base,
    const unsigned char *input, unsigned blocks, const unsigned long long *order,
    unsigned long long offset, unsigned population, unsigned topk,
    unsigned column_base, int ordered, int *status, const unsigned *grid_table)
{
    __shared__ unsigned gate_stage[2][64u * 17u], up_stage[2][64u * 17u];
    __shared__ unsigned input_stage[2][16u * 73u];
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    unsigned group = lane >> 2u, thread = lane & 3u;
    unsigned warp_row = warp / 2u * 16u, warp_column = warp % 2u * 8u;
    unsigned long long first_row = row_base - warp_row;
    tensorcore_pair_tree trees[2] = {};
    unsigned slot = 0u;
    tensorcore_iq2_pair_stage(gate_stage[0], up_stage[0], input_stage[0], gate, up, rb,
        rows, first_row, input, blocks, 0u, order, offset, population, topk, column_base, ordered);
    for (unsigned leaf = 0u; leaf < 32u; ++leaf) {
        unsigned block = __brev(leaf) >> 27u;
        float4 values[2] = {};
        if (block < blocks) {
#if __CUDA_ARCH__ >= 800
            asm volatile("cp.async.wait_group 0;" ::: "memory");
#endif
            __syncthreads();
            unsigned next = leaf + 1u;
            while (next < 32u && (__brev(next) >> 27u) >= blocks) ++next;
            if (next < 32u)
                tensorcore_iq2_pair_stage(gate_stage[slot ^ 1u], up_stage[slot ^ 1u], input_stage[slot ^ 1u],
                    gate, up, rb, rows, first_row, input, blocks, __brev(next) >> 27u,
                    order, offset, population, topk, column_base, ordered);
            const unsigned *x = input_stage[slot];
            unsigned displacement = (block & 1u) * 2u;
            int products[2][4] = {};
#pragma unroll
            for (unsigned segment = 0u; segment < 256u; segment += 32u) {
                int b[2];
#pragma unroll
                for (unsigned j = 0u; j < 2u; ++j)
                    b[j] = (int)x[(warp_column + group) * 73u + 1u + segment / 4u + thread + j * 4u];
#pragma unroll
                for (unsigned which = 0u; which < 2u; ++which) {
                    const unsigned *w = which ? up_stage[slot] : gate_stage[slot];
                    int a[4], partial[4] = {};
#pragma unroll
                    for (unsigned j = 0u; j < 4u; ++j) {
                        unsigned row = warp_row + group + (j & 1u) * 8u;
                        unsigned first = segment + thread * 4u + (j >> 1u) * 16u;
                        a[j] = tensorcore_iq2_digit4(w + row * 17u, displacement, first, grid_table);
                    }
                    tensorcore_integer_mma(partial, a, b);
#pragma unroll
                    for (unsigned j = 0u; j < 4u; ++j) {
                        unsigned row = warp_row + group + (j >> 1u) * 8u;
                        unsigned ss = tensorcore_iq2_word(w + row * 17u, displacement + 6u + segment / 32u * 8u);
                        products[which][j] += partial[j] * (int)(2u * (ss >> 28u) + 1u);
                    }
                }
            }
#pragma unroll
            for (unsigned which = 0u; which < 2u; ++which) {
                const unsigned *w = which ? up_stage[slot] : gate_stage[slot];
                float scalars[4] = {};
#pragma unroll
                for (unsigned j = 0u; j < 4u; ++j) {
                    unsigned row = warp_row + group + (j >> 1u) * 8u;
                    unsigned column = warp_column + thread * 2u + (j & 1u);
                    if (first_row + row < rows && column_base + column < population) {
                        float as = __uint_as_float(x[column * 73u]);
                        float ws = f16_bits_to_float((w[row * 17u] >> (displacement * 8u)) & 65535u);
                        scalars[j] = 0.125f * ws * as * (float)products[which][j];
                    }
                }
                values[which] = make_float4(scalars[0], scalars[1], scalars[2], scalars[3]);
            }
            __syncthreads();
            slot ^= 1u;
        }
#pragma unroll
        for (unsigned which = 0u; which < 2u; ++which)
            tensorcore_pair_push(trees[which], values[which], leaf);
    }
    tensorcore_pair_result result = {trees[0].result, trees[1].result};
#pragma unroll
    for (unsigned which = 0u; which < 2u; ++which) {
        float4 value = which ? result.up : result.gate;
        float scalars[] = {value.x, value.y, value.z, value.w};
#pragma unroll
        for (unsigned j = 0u; j < 4u; ++j) {
            unsigned long long row = row_base + group + (j >> 1u) * 8u;
            unsigned column = column_base + warp_column + thread * 2u + (j & 1u);
            if (row < rows && column < population && !isfinite(scalars[j])) {
                unsigned long long source = ordered ? offset + column : order[offset + column] / topk;
                scalars[j] = qtype_q8_dot_recover_f64((which ? up : gate) + row * rb,
                    input + source * blocks * 292ull, blocks, 66ull, YVEX_GGUF_QTYPE_IQ2_XXS,
                    status, which ? YVEX_MOE_DEVICE_UP_OPERAND : YVEX_MOE_DEVICE_GATE_OPERAND);
            }
        }
        value = make_float4(scalars[0], scalars[1], scalars[2], scalars[3]);
        if (which) result.up = value; else result.gate = value;
    }
    return result;
}

static __device__ float4 tensorcore_expert_register_dot(const unsigned char *weights,unsigned long long rb,
    unsigned long long rows,unsigned long long rowbase,const unsigned char *input,
    unsigned blocks,const unsigned long long *order,unsigned long long offset,
    unsigned pop,unsigned topk,unsigned columnbase,unsigned qtype,int ordered,int *status,
    const unsigned int *grid_table, int operand_failure) {
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
            int minima[4] = {};
            if (qtype == YVEX_GGUF_QTYPE_Q2_K)
                tensorcore_q2_minimum(minima, weights, rb, rows, rowbase, input,
                    blocks, block, order, offset, pop, topk, columnbase, ordered);
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
                        value=as*f16_bits_to_float(qtype_load_u16(w+80))*(float)lo[j]-
                            as*f16_bits_to_float(qtype_load_u16(w+82))*(float)minima[j];
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
                qtype == YVEX_GGUF_QTYPE_IQ2_XXS ? 66ull : 84ull, qtype, status,
                operand_failure);
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
    unsigned int *column_base, int *status,
    const unsigned long long *matrix_tiles)
{
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long cursor = 0ull;
    *column_base = 0u;
    if (matrix_tiles) {
        if (tile >= matrix_tiles[0]) return buckets;
        unsigned long long descriptor = matrix_tiles[1ull + tile];
        unsigned long long bucket = descriptor >> 32u;
        *column_base = (unsigned int)descriptor;
        if (bucket >= buckets || (*column_base & 7u) ||
            populations[bucket] < minimum ||
            populations[bucket] > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH ||
            *column_base >= populations[bucket]) {
            if (!lane) atomicCAS(status, 0, 2);
            return buckets;
        }
        return bucket;
    }
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

template<bool COOPERATIVE, bool IQ2_COOPERATIVE = false>
static __device__ __forceinline__ void tensorcore_grouped_up(
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
    float *intermediate, int *status, const unsigned long long *matrix_tiles)
{
    /* IQ2 fragment lanes use different codebook entries. Stage the canonical
     * table once rather than serializing divergent constant-memory reads. */
    __shared__ unsigned int grid_table[512];
    __shared__ int active;
    if (!threadIdx.x) active = status && atomicAdd(status, 0) == 0;
    if ((!COOPERATIVE || IQ2_COOPERATIVE) &&
        (gate_qtype == YVEX_GGUF_QTYPE_IQ2_XXS || up_qtype == YVEX_GGUF_QTYPE_IQ2_XXS))
        tensorcore_iq2_grid(grid_table);
    __syncthreads();
    if (!active) return;
    unsigned int warp = threadIdx.x >> 5u, lane = threadIdx.x & 31u;
    constexpr bool cooperative = COOPERATIVE;
    const unsigned paired_qtype = IQ2_COOPERATIVE ? YVEX_GGUF_QTYPE_IQ2_XXS : YVEX_GGUF_QTYPE_Q2_K;
    if (cooperative && (!matrix_tiles || gate_qtype != paired_qtype || up_qtype != paired_qtype ||
        (IQ2_COOPERATIVE && (input_width / 256ull % 2ull ||
            (((unsigned long long)gate | (unsigned long long)up |
              (unsigned long long)input | gate_row_bytes | up_row_bytes) & 3ull))))) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long tiles = (intermediate_width + 15ull) / 16ull;
    unsigned long long task = cooperative ? blockIdx.x : (unsigned long long)blockIdx.x * 4ull + warp;
    unsigned long long row_base = tiles ? (task % tiles) * 16ull : intermediate_width;
    if (blockDim.x != (cooperative ? 256u : 128u) || !summary || !expert_ids ||
        !bucket_offsets || !bucket_populations) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    if (!tiles || !tensor_core_minimum || summary->bucket_count > expert_count ||
        summary->bucket_count > pair_count || (matrix_tiles && matrix_tiles[0] >
            pair_count / 8ull + (pair_count % 8ull != 0ull) + summary->bucket_count)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    /* Adjacent warps visit column tiles before advancing the output row
     * tile, making same-expert encoded weight rows reusable in cache. */
    if (matrix_tiles) {
        if (!matrix_tiles[0]) return;
        row_base = (task / matrix_tiles[0]) * (cooperative ? 64ull : 16ull);
        if (row_base >= intermediate_width) return;
    }
    unsigned int column_base = 0u;
    unsigned long long bucket = tensorcore_expert_column_tile(
        matrix_tiles ? task % matrix_tiles[0] : task / tiles,
        bucket_populations, summary->bucket_count, tensor_core_minimum, &column_base, status,
        matrix_tiles);
    if (bucket >= summary->bucket_count) return;
    /* A paired tile owns two adjacent canonical eight-column descriptors.
     * Odd descriptors perform no loads or stores; no output is duplicated. */
    if (cooperative && (column_base & 8u)) return;
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
                   column < population && column < column_base + (cooperative ? 16ull : 8ull); ++column) {
        unsigned long long source_pair = order[offset + column];
        if (source_pair >= pair_count || selected[source_pair] != expert)
            atomicCAS(status, 0, 2);
    }
    if (cooperative) {
        /* Every warp validates the same eight pairs; thread zero therefore
         * observes all local refusals. Broadcast one atomic read, not 128. */
        if (__syncthreads_or(!threadIdx.x && atomicAdd(status, 0) != 0)) return;
        row_base += warp / 2u * 16u;
    } else {
        __syncwarp();
        if (*status) return;
    }
    if (!tensorcore_row_geometry(gate_qtype, input_width, gate_row_bytes) ||
        !tensorcore_row_geometry(up_qtype, input_width, up_row_bytes) ||
        gate_expert_bytes != intermediate_width * gate_row_bytes ||
        up_expert_bytes != intermediate_width * up_row_bytes) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    {
        float4 gate_values, up_values;
        if (IQ2_COOPERATIVE) {
            tensorcore_pair_result pair = tensorcore_iq2_pair_dot(
                gate + expert * gate_expert_bytes, up + expert * up_expert_bytes,
                gate_row_bytes, intermediate_width, row_base, input, (unsigned int)input_blocks,
                order, offset, (unsigned int)population, (unsigned int)topk,
                column_base, 0, status, grid_table);
            gate_values = pair.gate;
            up_values = pair.up;
        } else if (cooperative) {
            tensorcore_pair_result pair = tensorcore_pair_dot<16u>(
                gate + expert * gate_expert_bytes, up + expert * up_expert_bytes,
                gate_row_bytes, intermediate_width, row_base, input, (unsigned int)input_blocks,
                order, offset, (unsigned int)population, (unsigned int)topk,
                column_base, 0, status);
            gate_values = pair.gate;
            up_values = pair.up;
        } else {
            gate_values = tensorcore_expert_register_dot(gate + expert * gate_expert_bytes,
                gate_row_bytes, intermediate_width, row_base, input, (unsigned int)input_blocks,
                order, offset, (unsigned int)population, (unsigned int)topk,
                column_base, gate_qtype, 0, status, grid_table, YVEX_MOE_DEVICE_GATE_OPERAND);
            up_values = tensorcore_expert_register_dot(up + expert * up_expert_bytes,
                up_row_bytes, intermediate_width, row_base, input, (unsigned int)input_blocks,
                order, offset, (unsigned int)population, (unsigned int)topk,
                column_base, up_qtype, 0, status, grid_table, YVEX_MOE_DEVICE_UP_OPERAND);
        }
        const float gates[] = {gate_values.x, gate_values.y, gate_values.z, gate_values.w};
        const float ups[] = {up_values.x, up_values.y, up_values.z, up_values.w};
        for (unsigned int coordinate = 0u; coordinate < 4u; ++coordinate) {
            unsigned long long row = row_base + (lane >> 2u) + (coordinate >> 1u) * 8ull;
            unsigned int column = column_base + (cooperative ? warp % 2u * 8u : 0u) +
                (lane & 3u) * 2u + (coordinate & 1u);
            if (row < intermediate_width && column < population && !*status) {
                unsigned long long ordered_pair = offset + column, source_pair = order[ordered_pair];
                float value;
                if (!cuda_clamped_swiglu_bf16_value(gates[coordinate], ups[coordinate],
                        limit, weights[source_pair], &value))
                    atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTED_ACTIVATION);
                else intermediate[ordered_pair * intermediate_width + row] = value;
            }
        }
    }
}

template<bool COOPERATIVE>
static __device__ __forceinline__ void tensorcore_grouped_down(
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
    unsigned long long hidden, float *pair_outputs, int *status,
    const unsigned long long *matrix_tiles)
{
    __shared__ int active;
    if (!threadIdx.x) active = status && atomicAdd(status, 0) == 0;
    __syncthreads();
    if (!active) return;
    unsigned int warp = threadIdx.x >> 5u, lane = threadIdx.x & 31u;
    constexpr bool cooperative = COOPERATIVE;
    if (cooperative && (!matrix_tiles || qtype != YVEX_GGUF_QTYPE_Q2_K)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long tiles = (hidden + 15ull) / 16ull;
    unsigned long long task = cooperative ? blockIdx.x : (unsigned long long)blockIdx.x * 4ull + warp;
    unsigned long long row_base = tiles ? (task % tiles) * 16ull : hidden;
    if (blockDim.x != 128u || !summary || !expert_ids || !bucket_offsets || !bucket_populations) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    if (!tiles || !tensor_core_minimum || summary->bucket_count > expert_count ||
        summary->bucket_count > pair_count || (matrix_tiles && matrix_tiles[0] >
            pair_count / 8ull + (pair_count % 8ull != 0ull) + summary->bucket_count)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    if (matrix_tiles) {
        if (!matrix_tiles[0]) return;
        row_base = (task / matrix_tiles[0]) * (cooperative ? 64ull : 16ull);
        if (row_base >= hidden) return;
    }
    unsigned int column_base = 0u;
    unsigned long long bucket = tensorcore_expert_column_tile(
        matrix_tiles ? task % matrix_tiles[0] : task / tiles,
        bucket_populations, summary->bucket_count, tensor_core_minimum, &column_base, status,
        matrix_tiles);
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
    if (cooperative) {
        if (__syncthreads_or(!threadIdx.x && atomicAdd(status, 0) != 0)) return;
        row_base += warp * 16u;
    } else {
        __syncwarp();
        if (*status) return;
    }
    if (!tensorcore_row_geometry(qtype, intermediate_width, row_bytes) ||
        expert_bytes != hidden * row_bytes) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    {
        float4 dot = cooperative ? tensorcore_expert_shared_dot(down + expert * expert_bytes,
            row_bytes, hidden, row_base, intermediate, (unsigned int)input_blocks,
            order, offset, (unsigned int)population, (unsigned int)topk, column_base, 1, status) :
            tensorcore_expert_register_dot(down + expert * expert_bytes,
            row_bytes, hidden, row_base, intermediate, (unsigned int)input_blocks,
            order, offset, (unsigned int)population, (unsigned int)topk, column_base, qtype, 1,
            status, nullptr, YVEX_MOE_DEVICE_DOWN_OPERAND);
        const float values[] = {dot.x, dot.y, dot.z, dot.w};
        for (unsigned int coordinate = 0u; coordinate < 4u; ++coordinate) {
            unsigned long long row = row_base + (lane >> 2u) + (coordinate >> 1u) * 8ull;
            unsigned int column = column_base + (lane & 3u) * 2u + (coordinate & 1u);
            if (row < hidden && column < population && !*status) {
                float value = float_to_bf16_rne(values[coordinate]);
                if (!isfinite(value)) atomicCAS(status, 0, YVEX_MOE_DEVICE_DOWN_PUBLICATION);
                else pair_outputs[order[offset + column] * hidden + row] = value;
            }
        }
    }
    if (!lane && !row_base && !*status)
        atomicAdd(&summary->matrix_tile_executed_pairs,
                  population - column_base < 8ull ? population - column_base : 8ull);
}

/* Keep the ROW class's eight fmaf terms per K256 block and 32-leaf tree,
 * while a 64x32 CTA reuses aligned encoded words and Q8 activations. The
 * DEFAULT matrix reduction above is intentionally not substituted. */
extern "C" __global__ __launch_bounds__(256,2) void yvex_mxfp4_row_matrix_shared(
    const unsigned char *encoded,unsigned long long row_bytes,unsigned long long row_width,
    unsigned long long start_row,unsigned long long row_count,unsigned long long input_rows,
    unsigned qtype,const unsigned char *activation,unsigned long long stride,
    int q8,int block_row,int forensic,const float *additive,float *output,
    unsigned long long output_stride,int output_bf16,int *status)
{
    const unsigned long long group_count=1ull,group_rows=row_count;
    __shared__ unsigned weight_words[64 * 34], activation_words[32 * 73];
    __shared__ int active;
    if (!status) return;
    if (!threadIdx.x) active = *status == 0;
    __syncthreads();
    if (!active) return;
    if (!encoded || !activation || !output || !row_count || row_count>131072ull ||
        !row_bytes || start_row>~0ull-row_count ||
        start_row+row_count>~0ull/row_bytes ||
        input_rows<32ull || input_rows>YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH || row_width>8192ull || q8!=1 ||
        stride!=row_width || block_row || forensic || output_stride<row_count ||
        output_stride>~0ull/input_rows || (output_bf16!=0 && output_bf16!=1) ||
        !group_count || !group_rows ||
        group_count > ~0ull / group_rows || row_count != group_count * group_rows ||
        blockDim.x != 256u || input_rows > ~0ull - 31ull ||
        (group_count > 1 && group_rows % 64) || qtype != YVEX_GGUF_QTYPE_MXFP4 ||
        !tensorcore_row_geometry(qtype, row_width, row_bytes)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    const unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    const unsigned long long input_groups = (input_rows + 31) / 32;
    const unsigned long long row_base = (blockIdx.x / input_groups) * 64;
    const unsigned long long input_base = (blockIdx.x % input_groups) * 32;
    if (row_base >= row_count) return;
    const unsigned long long row_group = row_base / group_rows, blocks = row_width / 256;
    const unsigned char *w = (const unsigned char *)weight_words;
    const unsigned char *x = (const unsigned char *)activation_words;
    tensorcore_pair_tree trees[2]={};
    for(unsigned leaf=0;leaf<32;++leaf) {
        unsigned block=__brev(leaf)>>27u;
        float totals[2][4]={};
        if(block<blocks) {
        for (unsigned i = threadIdx.x; i < 64 * 34; i += 256) {
            unsigned long long row = row_base + i / 34;
            weight_words[i] = row < row_count ? qtype_load_u32(
                encoded + (start_row + row) * row_bytes + block * 136 + (i % 34) * 4) : 0;
        }
        for (unsigned i = threadIdx.x; i < 32 * 73; i += 256) {
            unsigned long long column = input_base + i / 73;
            activation_words[i] = column < input_rows ? qtype_load_u32(activation +
                ((column * group_count + row_group) * blocks + block) * 292 + (i % 73) * 4) : 0;
        }
        __syncthreads();
#pragma unroll
        for (unsigned segment = 0; segment < 8; segment++) {
            int a[4];
#pragma unroll
            for (unsigned j = 0; j < 4; j++) {
                unsigned r = (warp / 2) * 16 + (lane >> 2) + (j & 1) * 8;
                unsigned first = (lane & 3) * 4 + (j >> 1) * 16;
                unsigned offset = r * 136 + segment * 17 + 1 + (first & 15u);
                unsigned index = offset >> 2, shift = (offset & 3u) * 8u;
                unsigned packed = __funnelshift_r(weight_words[index],
                    weight_words[index + (shift != 0)], shift);
                a[j] = mxfp4_i8x4(packed, first >= 16u);
            }
#pragma unroll
            for (unsigned tile = 0; tile < 2; tile++) {
                int b[2], product[4] = {};
                unsigned column = (warp % 2) * 16 + tile * 8 + (lane >> 2);
#pragma unroll
                for (unsigned j = 0; j < 2; j++)
                    b[j] = (int)qtype_load_u32(x + column * 292 + 4 + segment * 32 + (lane & 3) * 4 + j * 16);
                tensorcore_integer_mma(product, a, b);
#pragma unroll
                for (unsigned j = 0; j < 4; j++) {
                    unsigned r = (warp / 2) * 16 + (lane >> 2) + (j >> 1) * 8;
                    unsigned c = (warp % 2) * 16 + tile * 8 + (lane & 3) * 2 + (j & 1);
                    float scale = __uint_as_float(activation_words[c * 73]);
                    float ws=e8m0_bits_to_float(w[r*136+segment*17]);
                    totals[tile][j]=fmaf(ws*0.5f*scale,(float)product[j],totals[tile][j]);
                }
            }
        }
        __syncthreads();
        }
        for(unsigned tile=0;tile<2;++tile)
            tensorcore_pair_push(trees[tile],make_float4(totals[tile][0],totals[tile][1],
                totals[tile][2],totals[tile][3]),leaf);
    }
    float totals[2][4]={{trees[0].result.x,trees[0].result.y,trees[0].result.z,trees[0].result.w},
                        {trees[1].result.x,trees[1].result.y,trees[1].result.z,trees[1].result.w}};
#pragma unroll
    for (unsigned tile = 0; tile < 2; tile++) {
#pragma unroll
        for (unsigned j = 0; j < 4; j++) {
            unsigned long long row = row_base + (warp / 2) * 16 + (lane >> 2) + (j >> 1) * 8;
            unsigned long long column = input_base + (warp % 2) * 16 + tile * 8 + (lane & 3) * 2 + (j & 1);
            if (row < row_count && column < input_rows) {
                unsigned long long i = column * output_stride + row;
                float value=totals[tile][j];
                if(!isfinite(value))value=qtype_q8_dot_recover_f64(
                    encoded+(start_row+row)*row_bytes,activation+column*blocks*292ull,
                    blocks,136ull,YVEX_GGUF_QTYPE_MXFP4,status);
                if(additive)value=__fadd_rn(value,additive[i]);
                if(output_bf16)value=float_to_bf16_rne(value);
                if(!isfinite(value))atomicCAS(status,0,1);
                else output[i]=value;
            }
        }
    }
}

/* Separate entrypoints keep register/shared admission local to the selected
 * realization. The template above remains the single worklist/numerical owner. */
extern "C" __global__ void yvex_moe_grouped_up_tensorcore(
    const unsigned char *gate, unsigned long long gate_row_bytes,
    unsigned long long gate_expert_bytes, unsigned gate_qtype,
    const unsigned char *up, unsigned long long up_row_bytes,
    unsigned long long up_expert_bytes, unsigned up_qtype,
    const unsigned long long *selected, const float *weights, const unsigned long long *order,
    const unsigned long long *expert_ids, const unsigned long long *offsets,
    const unsigned long long *populations, const yvex_expert_worklist_observation *summary,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    unsigned long long minimum, const unsigned char *input, unsigned long long width,
    unsigned long long intermediate_width, double limit, float *output, int *status,
    const unsigned long long *tiles)
{
    tensorcore_grouped_up<false>(gate, gate_row_bytes, gate_expert_bytes, gate_qtype,
        up, up_row_bytes, up_expert_bytes, up_qtype, selected, weights, order, expert_ids,
        offsets, populations, summary, pairs, topk, experts, minimum, input, width,
        intermediate_width, limit, output, status, tiles);
}
/* Bound register residency independently of the larger legacy IQ2 consumer.
 * This equivalent CTA implementation retains its own launch resource class. */
extern "C" __global__ __launch_bounds__(256, 2) void yvex_moe_grouped_up_shared(
    const unsigned char *gate, unsigned long long gate_row_bytes,
    unsigned long long gate_expert_bytes, unsigned gate_qtype,
    const unsigned char *up, unsigned long long up_row_bytes,
    unsigned long long up_expert_bytes, unsigned up_qtype,
    const unsigned long long *selected, const float *weights, const unsigned long long *order,
    const unsigned long long *expert_ids, const unsigned long long *offsets,
    const unsigned long long *populations, const yvex_expert_worklist_observation *summary,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    unsigned long long minimum, const unsigned char *input, unsigned long long width,
    unsigned long long intermediate_width, double limit, float *output, int *status,
    const unsigned long long *tiles)
{
    tensorcore_grouped_up<true>(gate, gate_row_bytes, gate_expert_bytes, gate_qtype,
        up, up_row_bytes, up_expert_bytes, up_qtype, selected, weights, order, expert_ids,
        offsets, populations, summary, pairs, topk, experts, minimum, input, width,
        intermediate_width, limit, output, status, tiles);
}
extern "C" __global__ __launch_bounds__(256, 2) void yvex_moe_grouped_up_iq2_shared(
    const unsigned char *gate, unsigned long long gate_row_bytes,
    unsigned long long gate_expert_bytes, unsigned gate_qtype,
    const unsigned char *up, unsigned long long up_row_bytes,
    unsigned long long up_expert_bytes, unsigned up_qtype,
    const unsigned long long *selected, const float *weights, const unsigned long long *order,
    const unsigned long long *expert_ids, const unsigned long long *offsets,
    const unsigned long long *populations, const yvex_expert_worklist_observation *summary,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    unsigned long long minimum, const unsigned char *input, unsigned long long width,
    unsigned long long intermediate_width, double limit, float *output, int *status,
    const unsigned long long *tiles)
{
    tensorcore_grouped_up<true, true>(gate, gate_row_bytes, gate_expert_bytes, gate_qtype,
        up, up_row_bytes, up_expert_bytes, up_qtype, selected, weights, order, expert_ids,
        offsets, populations, summary, pairs, topk, experts, minimum, input, width,
        intermediate_width, limit, output, status, tiles);
}

extern "C" __global__ void yvex_moe_grouped_down_tensorcore(
    const unsigned char *down, unsigned long long row_bytes, unsigned long long expert_bytes,
    unsigned qtype, const unsigned long long *selected, const unsigned long long *order,
    const unsigned long long *expert_ids, const unsigned long long *offsets,
    const unsigned long long *populations, yvex_expert_worklist_observation *summary,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    unsigned long long minimum, const unsigned char *input, unsigned long long width,
    unsigned long long hidden, float *output, int *status, const unsigned long long *tiles)
{
    tensorcore_grouped_down<false>(down, row_bytes, expert_bytes, qtype, selected, order,
        expert_ids, offsets, populations, summary, pairs, topk, experts, minimum,
        input, width, hidden, output, status, tiles);
}
extern "C" __global__ void yvex_moe_grouped_down_shared(
    const unsigned char *down, unsigned long long row_bytes, unsigned long long expert_bytes,
    unsigned qtype, const unsigned long long *selected, const unsigned long long *order,
    const unsigned long long *expert_ids, const unsigned long long *offsets,
    const unsigned long long *populations, yvex_expert_worklist_observation *summary,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    unsigned long long minimum, const unsigned char *input, unsigned long long width,
    unsigned long long hidden, float *output, int *status, const unsigned long long *tiles)
{
    tensorcore_grouped_down<true>(down, row_bytes, expert_bytes, qtype, selected, order,
        expert_ids, offsets, populations, summary, pairs, topk, experts, minimum,
        input, width, hidden, output, status, tiles);
}
