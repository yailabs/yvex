/* Private-primitive numerical conformance: host and literal-device oracles are separate
 * from the producer certificate. No production ABI or model is involved. Include the actual private implementation
 * only in this standalone test translation unit; neither oracle calls it. */
#include <cuda_runtime.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include <vector>
#include "src/backend/cuda/sampling_kernels.cu"

#define CHECK(call) do { cudaError_t rc = (call); if (rc != cudaSuccess) { \
    std::fprintf(stderr, "%s: %s\n", #call, cudaGetErrorString(rc)); std::exit(1); } } while (0)

__global__ void probe(const double *input, unsigned long long count,
                      double *output, int *flags)
{
    __shared__ int valid;
    if (!threadIdx.x) valid = 1;
    __syncthreads();
    double candidate = 0.0;
    int accepted = sampling_sum_certificate_device(input, count, &valid, &candidate);
    double actual = sampling_ordered_sum_device(input, count, &valid);
    if (!threadIdx.x) {
        double oracle = 0.0, errors = 0.0;
        int oracle_valid = 1;
        for (unsigned long long i = 0ull; i < count; ++i) {
            double x = input[i];
            if (!isfinite(x) || x < 0.0) { oracle_valid = 0; continue; }
            double previous = oracle;
            oracle = __dadd_rn(previous, x);
            double residual = fabs(previous) >= fabs(x)
                ? __dadd_rn(__dsub_rn(previous, oracle), x)
                : __dadd_rn(__dsub_rn(x, oracle), previous);
            errors = __dadd_rn(errors, residual);
        }
        output[0] = actual;
        output[1] = __dadd_rn(oracle, errors);
        output[2] = candidate;
        flags[0] = valid; flags[1] = oracle_valid; flags[2] = accepted;
    }
}

static uint64_t bits(double x)
{
    uint64_t result;
    std::memcpy(&result, &x, sizeof(result));
    return result;
}

static uint64_t random_bits(uint64_t &state)
{
    state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
    return state * UINT64_C(2685821657736338717);
}

static double host_literal(const std::vector<double> &input, int &valid)
{
    double high = 0.0, low = 0.0;
    valid = 1;
    for (double x : input) {
        if (!std::isfinite(x) || x < 0.0) { valid = 0; continue; }
        double next = high + x;
        double error = std::fabs(high) >= std::fabs(x)
            ? (high - next) + x : (x - next) + high;
        low += error;
        high = next;
    }
    return high + low;
}

__global__ void probe_initialization(const float *input, unsigned long long count,
    double temperature, unsigned int *tokens, float *logits, double *probabilities,
    double *deviations, double *maximum, float *facts, int *status)
{
    sampling_initialize_device(input, count, temperature, tokens, logits,
        probabilities, deviations, maximum, facts, status);
}

static int initialization_controls(void)
{
    const size_t counts[] = {1u,31u,256u,1025u,129283u};
    for (size_t count : counts) for (unsigned pattern = 0u; pattern < 8u; ++pattern) {
        std::vector<float> input(count), observed(count);
        std::vector<double> scaled(count), deviations(count);
        std::vector<unsigned int> tokens(count);
        double temperature = pattern == 5u ? 1e308 : pattern == 6u ? 0x1p-1022 : 1.0;
        for (size_t i = 0u; i < count; ++i) {
            input[i] = (float)((int)(i % 29u) - 14) * 0.125f;
            if (pattern == 1u) input[i] = -0.0f;
            if (pattern == 2u) input[i] = i ? 0.0f : -0.0f;
            if (pattern == 3u) input[i] = i ? -0.0f : 0.0f;
            if (pattern == 4u) input[i] = -FLT_MAX;
            if (pattern == 5u) input[i] = -0x1p-149f;
            if (pattern == 6u) input[i] = FLT_MAX;
        }
        if (pattern == 7u) input[count / 2u] = NAN;
        float literal_logit = -FLT_MAX;
        double literal_scaled = -DBL_MAX;
        bool admitted = true;
        for (float value : input) {
            double value_scaled = (double)value / temperature;
            if (!std::isfinite(value) || !std::isfinite(value_scaled)) admitted = false;
            if (value > literal_logit) literal_logit = value;
            if (value_scaled > literal_scaled) literal_scaled = value_scaled;
        }
        float *device, *device_logits, *facts;
        double *device_scaled, *device_deviations, *maximum;
        unsigned int *device_tokens; int *status;
        CHECK(cudaMalloc(&device, count * sizeof(float)));
        CHECK(cudaMalloc(&device_logits, count * sizeof(float)));
        CHECK(cudaMalloc(&facts, 16u * sizeof(float)));
        CHECK(cudaMalloc(&device_scaled, count * sizeof(double)));
        CHECK(cudaMalloc(&device_deviations, count * sizeof(double)));
        CHECK(cudaMalloc(&maximum, sizeof(double)));
        CHECK(cudaMalloc(&device_tokens, count * sizeof(unsigned int)));
        CHECK(cudaMalloc(&status, sizeof(int)));
        CHECK(cudaMemset(status, 0, sizeof(int)));
        CHECK(cudaMemcpy(device, input.data(), count*sizeof(float), cudaMemcpyHostToDevice));
        probe_initialization<<<1u,256u>>>(device, count, temperature, device_tokens,
            device_logits, device_scaled, device_deviations, maximum, facts, status);
        CHECK(cudaGetLastError()); CHECK(cudaDeviceSynchronize());
        int refused; double actual_maximum; float actual_logit;
        CHECK(cudaMemcpy(&refused, status, sizeof(refused), cudaMemcpyDeviceToHost));
        CHECK(cudaMemcpy(&actual_maximum, maximum, sizeof(double), cudaMemcpyDeviceToHost));
        CHECK(cudaMemcpy(&actual_logit, facts+YVEX_CUDA_SAMPLING_MAXIMUM_LOGIT,
                         sizeof(float), cudaMemcpyDeviceToHost));
        if (admitted != (refused == 0)) return 1;
        if (admitted) {
            if (bits(actual_maximum) != bits(literal_scaled) ||
                std::memcmp(&actual_logit, &literal_logit, sizeof(float))) return 1;
            CHECK(cudaMemcpy(observed.data(), device_logits, count*sizeof(float), cudaMemcpyDeviceToHost));
            CHECK(cudaMemcpy(scaled.data(), device_scaled, count*sizeof(double), cudaMemcpyDeviceToHost));
            CHECK(cudaMemcpy(deviations.data(), device_deviations, count*sizeof(double), cudaMemcpyDeviceToHost));
            CHECK(cudaMemcpy(tokens.data(), device_tokens, count*sizeof(unsigned int), cudaMemcpyDeviceToHost));
            for (size_t i = 0u; i < count; ++i) {
                double expected = (double)input[i] / temperature;
                if (tokens[i] != i || std::memcmp(&observed[i], &input[i], sizeof(float)) ||
                    bits(scaled[i]) != bits(expected) || bits(deviations[i]) != 0ull) return 1;
            }
        }
        CHECK(cudaFree(status)); CHECK(cudaFree(device_tokens)); CHECK(cudaFree(maximum));
        CHECK(cudaFree(device_deviations)); CHECK(cudaFree(device_scaled)); CHECK(cudaFree(facts));
        CHECK(cudaFree(device_logits)); CHECK(cudaFree(device));
    }
    std::fprintf(stderr, "PASS 40 literal initialization/max/tie/refusal controls\n");
    return 0;
}

/* At one, the upper half-cell is twice the lower half-cell. The positive
 * interior case is certifiable only with the separate signed endpoint test;
 * neither midpoint may be accepted, even though ties round to the even one. */
static int rounding_cell_controls(void)
{
    const size_t counts[] = {1024u, 129283u};
    for (size_t count : counts) for (unsigned pattern = 0u; pattern < 4u; ++pattern) {
        std::vector<double> input(count, 0.0);
        input[0] = pattern < 2u ? 1.0 : std::nextafter(1.0, 0.0);
        if (pattern == 1u) input[1] = 0x1p-53;
        else if (pattern == 2u) input[1] = 0x1p-54;
        else input[1] = input[2] = input[3] = 0x1p-55;
        double *device, *result;
        int *flags;
        CHECK(cudaMalloc(&device, count * sizeof(double)));
        CHECK(cudaMalloc(&result, 3u * sizeof(double)));
        CHECK(cudaMalloc(&flags, 3u * sizeof(int)));
        CHECK(cudaMemcpy(device, input.data(), count*sizeof(double), cudaMemcpyHostToDevice));
        probe<<<1u,256u>>>(device, count, result, flags);
        CHECK(cudaGetLastError()); CHECK(cudaDeviceSynchronize());
        double observed[3]; int actual_flags[3], valid;
        CHECK(cudaMemcpy(observed, result, sizeof(observed), cudaMemcpyDeviceToHost));
        CHECK(cudaMemcpy(actual_flags, flags, sizeof(actual_flags), cudaMemcpyDeviceToHost));
        double literal = host_literal(input, valid);
        bool interior = pattern == 0u || pattern == 3u;
        if (!valid || !actual_flags[0] || !actual_flags[1] ||
            bits(literal) != bits(1.0) || bits(observed[0]) != bits(literal) ||
            bits(observed[1]) != bits(literal) || bool(actual_flags[2]) != interior ||
            (interior && bits(observed[2]) != bits(literal))) return 1;
        CHECK(cudaFree(flags)); CHECK(cudaFree(result)); CHECK(cudaFree(device));
    }
    std::fprintf(stderr, "PASS 8 asymmetric rounding-cell/interior/midpoint controls\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    if (initialization_controls() || rounding_cell_controls()) return 1;
    FILE *raw = std::fopen(argv[1], "wbx");
    if (!raw) return 2;
    const size_t counts[] = {1u,31u,255u,256u,1023u,1024u,1025u,4097u,129280u,129283u};
    unsigned int certified = 0u, fallback = 0u;
    for (size_t extent : counts) for (unsigned int pattern = 0u; pattern < 10u; ++pattern) {
        std::vector<double> values(extent, 0.0);
        uint64_t state = UINT64_C(0x3790174050108) + extent * 7u + pattern;
        for (size_t i = 0u; i < extent; ++i) {
            uint64_t value = random_bits(state);
            if (pattern == 0u) values[i] = 1.0;
            if (pattern == 1u) values[i] = std::ldexp(1.0, -(int)(value % 1075u));
            if (pattern == 2u) values[i] = std::ldexp(1.0 + (double)(value & 0xffffu) / 65536.0,
                -(int)(value % 1024u));
            if (pattern == 3u) values[i] = i % 17u ? 0.0 : std::ldexp(1.0, -(int)(value % 900u));
            if (pattern == 4u) values[i] = std::ldexp(1.0, -1074);
        }
        if (pattern == 5u || pattern == 6u) {
            values[0] = 1.0;
            if (extent > 1u) values[1] = std::ldexp(1.0, -54);
            if (extent > 2u) values[2] = std::ldexp(1.0, -54);
            if (pattern == 6u && extent > 3u) values[3] = std::ldexp(1.0, -1074);
        }
        if (pattern >= 7u) {
            values[0] = 1.0;
            values[extent / 2u] = pattern == 7u ? -0.5 : pattern == 8u ? NAN : INFINITY;
        }
        double *device, *result;
        int *flags;
        CHECK(cudaMalloc(&device, values.size() * sizeof(double)));
        CHECK(cudaMalloc(&result, 3u * sizeof(double)));
        CHECK(cudaMalloc(&flags, 3u * sizeof(int)));
        CHECK(cudaMemcpy(device, values.data(), values.size()*sizeof(double), cudaMemcpyHostToDevice));
        probe<<<1u,256u>>>(device, values.size(), result, flags);
        CHECK(cudaGetLastError()); CHECK(cudaDeviceSynchronize());
        double observed[3]; int actual_flags[3], host_valid;
        CHECK(cudaMemcpy(observed,result,sizeof(observed),cudaMemcpyDeviceToHost));
        CHECK(cudaMemcpy(actual_flags,flags,sizeof(actual_flags),cudaMemcpyDeviceToHost));
        double reference = host_literal(values, host_valid);
        if (bits(reference) != bits(observed[0]) || bits(reference) != bits(observed[1]) ||
            actual_flags[0] != host_valid || actual_flags[1] != host_valid ||
            (actual_flags[2] && bits(reference) != bits(observed[2]))) return 1;
        certified += actual_flags[2] != 0; fallback += actual_flags[2] == 0;
        uint64_t record[] = {extent,pattern,bits(reference),bits(observed[0]),
            (uint64_t)actual_flags[2],(uint64_t)host_valid};
        if (std::fwrite(record,sizeof(record),1u,raw)!=1u ||
            std::fwrite(values.data(),sizeof(double),values.size(),raw)!=values.size()) return 1;
        std::printf("{\"count\":%zu,\"pattern\":%u,\"literal_bits\":\"%016llx\",\"certificate\":%d}\n",
            extent,pattern,(unsigned long long)bits(reference),actual_flags[2]);
        CHECK(cudaFree(flags)); CHECK(cudaFree(result)); CHECK(cudaFree(device));
    }
    if (std::fclose(raw) != 0 || !certified || !fallback) return 1;
    std::fprintf(stderr,"PASS exact binary64 controls: certified=%u fallback=%u\n",certified,fallback);
    return 0;
}
