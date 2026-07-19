/* Owner: gguf.quant compute (TRACK.QUANT).
 * Owns: independent error accumulation and block-at-a-time encoded row dots.
 * Does not own: qtype storage geometry, encoding policy, payload IO, CUDA, artifact writing, transformer graphs, or
 *   rendering.
 * Invariants: metrics never retain tensors; CPU compute consumes encoded blocks directly and rejects non-divisible
 *   rows and malformed byte counts.
 * Boundary: a qtype row dot is bounded compute evidence, not model execution.
 * Purpose: accumulate independent numeric error facts and evaluate encoded row dots.
 * Inputs: caller-owned scalar slices, encoded qtype blocks, and immutable geometry facts.
 * Effects: mutates only caller-owned metrics, outputs, failures, and error state.
 * Failure: invalid geometry, non-finite arithmetic, and overflow refuse without partial output. */
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <yvex/internal/quant_numeric.h>

/* Purpose: reset one caller-owned metric accumulator to its canonical empty state.
 * Inputs: an optional writable metric record.
 * Effects: clears the record when present.
 * Failure: none; a null record is a no-op.
 * Boundary: initialization does not validate or execute a codec. */
void yvex_quant_metrics_init(yvex_quant_metrics *metrics) {
    if (metrics)
        memset(metrics, 0, sizeof(*metrics));
}

/* Purpose: add one finite term while rejecting non-finite accumulation. */
static int quant_metrics_add(double *total, double term) {
    double next;

    if (!total || !isfinite(*total) || !isfinite(term))
        return 0;
    next = *total + term;
    if (!isfinite(next))
        return 0;
    *total = next;
    return 1;
}

/* Purpose: compare a bounded reconstructed slice against an independent reference slice.
 * Inputs: mutable metrics, equal-length value arrays, an optional dot vector, and element count.
 * Effects: atomically replaces the accumulator after every term passes finite arithmetic checks.
 * Failure: null inputs, count overflow, or non-finite derived arithmetic leave metrics unchanged.
 * Boundary: metrics characterize codec error; they do not admit an artifact or runtime path. */
int yvex_quant_metrics_update(yvex_quant_metrics *metrics, const float *reference,
                              const float *reconstructed, const float *dot_vector,
                              unsigned long long count) {
    yvex_quant_metrics next;
    unsigned long long index;

    if (!metrics || !reference || !reconstructed || count == 0u ||
        ULLONG_MAX - metrics->element_count < count || ULLONG_MAX - metrics->finite_count < count ||
        ULLONG_MAX - metrics->nonfinite_count < count)
        return 0;
    next = *metrics;
    for (index = 0u; index < count; ++index) {
        double expected = reference[index];
        double observed = reconstructed[index];
        double error;
        double absolute;
        double denominator;
        double dot = dot_vector ? dot_vector[index] : (double)((int)(index % 17u) - 8) / 8.0;

        if (!isfinite(expected) || !isfinite(observed)) {
            next.nonfinite_count++;
            continue;
        }
        if (!isfinite(dot))
            return 0;
        next.finite_count++;
        error = observed - expected;
        absolute = fabs(error);
        if (!isfinite(absolute))
            return 0;
        if (absolute > next.maximum_absolute_error)
            next.maximum_absolute_error = absolute;
        if (!quant_metrics_add(&next.absolute_error_sum, absolute) ||
            !quant_metrics_add(&next.squared_error_sum, error * error))
            return 0;
        denominator = fabs(expected);
        if (denominator < 1e-12)
            denominator = 1.0;
        if (!quant_metrics_add(&next.relative_error_sum, absolute / denominator) ||
            !quant_metrics_add(&next.reference_squared_sum, expected * expected) ||
            !quant_metrics_add(&next.dot_reference, expected * dot) ||
            !quant_metrics_add(&next.dot_reconstructed, observed * dot))
            return 0;
    }
    next.element_count += count;
    *metrics = next;
    return 1;
}

/* Purpose: derive mean absolute error from the finite observations in an accumulator.
 * Inputs: an optional immutable metric record.
 * Effects: none.
 * Failure: empty or null metrics yield zero by contract.
 * Boundary: this projection does not change numeric admission. */
double yvex_quant_metrics_mean_absolute_error(const yvex_quant_metrics *metrics) {
    return metrics && metrics->finite_count
               ? metrics->absolute_error_sum / (double)metrics->finite_count
               : 0.0;
}

/* Purpose: derive root mean square error from finite accumulated observations.
 * Inputs: an optional immutable metric record.
 * Effects: none.
 * Failure: empty or null metrics yield zero by contract.
 * Boundary: this scalar is diagnostic evidence, not a capability flag. */
double yvex_quant_metrics_rmse(const yvex_quant_metrics *metrics) {
    return metrics && metrics->finite_count
               ? sqrt(metrics->squared_error_sum / (double)metrics->finite_count)
               : 0.0;
}

/* Purpose: project the documented zero-policy relative-error mean.
 * Inputs: an optional immutable metric record.
 * Effects: none.
 * Failure: empty or null metrics yield zero by contract.
 * Boundary: the projection cannot select a physical encoding. */
double yvex_quant_metrics_mean_relative_error(const yvex_quant_metrics *metrics) {
    return metrics && metrics->finite_count
               ? metrics->relative_error_sum / (double)metrics->finite_count
               : 0.0;
}

/* Purpose: measure absolute divergence between reference and reconstructed dot products.
 * Inputs: an optional immutable metric record.
 * Effects: none.
 * Failure: a null record yields zero by contract.
 * Boundary: the result is bounded compute evidence only. */
double yvex_quant_metrics_dot_absolute_error(const yvex_quant_metrics *metrics) {
    return metrics ? fabs(metrics->dot_reconstructed - metrics->dot_reference) : 0.0;
}

/* Purpose: publish one typed CPU-compute refusal with expected and observed geometry.
 * Inputs: optional failure/error outputs plus qtype, code, status, and diagnostic facts.
 * Effects: replaces supplied failure and error records; no encoded bytes are modified.
 * Failure: always returns the supplied refusal status.
 * Boundary: centralizes row-dot refusal state without changing qtype support truth. */
static int quant_compute_fail(yvex_quant_failure *failure, yvex_quant_failure_code code,
                              unsigned int qtype, unsigned long long expected,
                              unsigned long long actual, yvex_error *err, int status,
                              const char *message) {
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->terminal_ordinal = ULLONG_MAX;
        failure->source_index = ULLONG_MAX;
        failure->row_index = ULLONG_MAX;
        failure->block_index = ULLONG_MAX;
        failure->expected = expected;
        failure->actual = actual;
        failure->qtype = qtype;
        failure->operation = YVEX_TRANSFORM_OP_COUNT;
    }
    yvex_error_set(err, (yvex_status)status, "quant.cpu_dot", message);
    return status;
}

/* Purpose: compute one encoded qtype row dot directly from canonical blocks.
 * Inputs: qtype, exact encoded bytes, an F32 vector, element count, and writable outputs.
 * Effects: decodes one block at a time and publishes the result only after complete validation.
 * Failure: unsupported qtypes, malformed rows, overflow, or non-finite terms return typed refusal.
 * Boundary: dedicated CPU qtype arithmetic is not tensor materialization or transformer execution. */
int yvex_quant_cpu_dot(unsigned int qtype, const unsigned char *encoded, size_t encoded_bytes,
                       const float *vector, unsigned long long elements, float *out,
                       yvex_quant_failure *failure, yvex_error *err) {
    const yvex_quant_numeric_capability *capability = yvex_quant_numeric_capability_at(qtype);
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(qtype);
    unsigned long long blocks;
    unsigned long long block;
    double sum = 0.0;
    float values[YVEX_QUANT_Q2_K_ELEMENTS];

    if (!encoded || !vector || !out || elements == 0u) {
        return quant_compute_fail(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, qtype, 1u, 0u, err,
                                  YVEX_ERR_INVALID_ARG,
                                  "encoded row, vector, element count, and result are required");
    }
    if (!capability || !capability->dedicated_cpu_compute_available || !geometry ||
        geometry->block_size == 0u || geometry->bytes_per_block == 0u) {
        return quant_compute_fail(failure, YVEX_QUANT_FAILURE_CPU_COMPUTE_UNAVAILABLE, qtype, 1u,
                                  0u, err, YVEX_ERR_UNSUPPORTED,
                                  "qtype has no dedicated CPU row-dot implementation");
    }
    if (elements % geometry->block_size != 0u) {
        return quant_compute_fail(failure, YVEX_QUANT_FAILURE_ROW_DIVISIBILITY, qtype,
                                  geometry->block_size, elements, err, YVEX_ERR_BOUNDS,
                                  "qtype row width is not block divisible");
    }
    blocks = elements / geometry->block_size;
    if (blocks > SIZE_MAX / geometry->bytes_per_block ||
        (size_t)blocks * geometry->bytes_per_block != encoded_bytes) {
        return quant_compute_fail(
            failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, qtype,
            blocks <= SIZE_MAX / geometry->bytes_per_block
                ? (unsigned long long)((size_t)blocks * geometry->bytes_per_block)
                : ULLONG_MAX,
            encoded_bytes, err, YVEX_ERR_BOUNDS, "encoded qtype row byte count is inconsistent");
    }
    for (block = 0u; block < blocks; ++block) {
        unsigned long long lane;
        int rc = yvex_quant_decode_block(qtype, encoded + (size_t)block * geometry->bytes_per_block,
                                         geometry->bytes_per_block, values, geometry->block_size,
                                         failure, err);
        if (rc != YVEX_OK) {
            if (failure)
                failure->block_index = block;
            return rc;
        }
        for (lane = 0u; lane < geometry->block_size; ++lane) {
            double term = (double)values[lane] * vector[block * geometry->block_size + lane];
            if (!isfinite(term) || !isfinite(sum + term))
                return quant_compute_fail(
                    failure, YVEX_QUANT_FAILURE_NONFINITE, qtype, 0u,
                    block * geometry->block_size + lane, err, YVEX_ERR_FORMAT,
                    "qtype row dot requires finite encoded values and vector");
            sum += term;
        }
    }
    *out = (float)sum;
    if (!isfinite(*out))
        return quant_compute_fail(failure, YVEX_QUANT_FAILURE_NUMERIC_BOUND, qtype, 0u, elements,
                                  err, YVEX_ERR_BOUNDS,
                                  "qtype row dot result is not representable as finite F32");
    if (failure)
        memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}
