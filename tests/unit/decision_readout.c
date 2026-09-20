/* Qualify finite-candidate likelihood arithmetic independently of model execution. */
#include "tests/test.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#include <yvex/internal/decision_readout.h>

static long double reference_log_probability(
    const float *logits, unsigned long long count, unsigned int token)
{
    long double maximum = (long double)logits[0], total = 0.0L;
    unsigned long long index;
    for (index = 1ull; index < count; ++index)
        if ((long double)logits[index] > maximum)
            maximum = (long double)logits[index];
    for (index = 0ull; index < count; ++index)
        total += expl((long double)logits[index] - maximum);
    return (long double)logits[token] - maximum - logl(total);
}

static int decision_readout_score_math(void)
{
    const float logits[] = {1.0f, -2.0f, 3.0f, 0.5f, -8.0f};
    const float nonfinite[] = {0.0f, NAN};
    long double expected = reference_log_probability(logits, 5ull, 2u);
    const long double tolerance = 2e-15L;
    double observed = 0.0;
    yvex_error err;

    YVEX_TEST_ASSERT(
        yvex_decision_readout_log_probability(
            logits, 5ull, 2u, &observed, &err) == YVEX_OK &&
            fabsl((long double)observed - expected) <= tolerance,
        "production stable log-softmax matches an independent long-double oracle");
    printf("decision-readout-math expected=%.21Lg observed=%.17g "
           "max_abs=%.21Lg tolerance=%.1Le\n",
           expected, observed, fabsl((long double)observed - expected),
           tolerance);
    YVEX_TEST_ASSERT(
        yvex_decision_readout_log_probability(
            logits, 5ull, 5u, &observed, &err) == YVEX_ERR_INVALID_ARG &&
            yvex_decision_readout_log_probability(
                nonfinite, 2ull, 0u, &observed, &err) == YVEX_ERR_FORMAT,
        "likelihood arithmetic refuses out-of-vocabulary and non-finite inputs");
    return 0;
}

static int decision_readout_distribution(void)
{
    yvex_decision_readout_candidate_result candidates[3] = {
        {.candidate_log_likelihood = -1.0},
        {.candidate_log_likelihood = -2.5},
        {.candidate_log_likelihood = -0.25}};
    long double maximum = -0.25L;
    long double denominator =
        expl(-1.0L - maximum) + expl(-2.5L - maximum) + 1.0L;
    long double observed_sum = 0.0L;
    const long double tolerance = 5e-16L;
    yvex_error err;
    unsigned long long index;

    YVEX_TEST_ASSERT(
        yvex_decision_readout_relative_distribution(
            candidates, 3ull, &err) == YVEX_OK,
        "finite score population produces one relative distribution");
    for (index = 0ull; index < 3ull; ++index)
        observed_sum += (long double)candidates[index]
                            .relative_candidate_probability;
    YVEX_TEST_ASSERT(
        fabsl(observed_sum - 1.0L) <= tolerance &&
            fabsl((long double)candidates[0].relative_candidate_probability -
                  expl(-1.0L - maximum) / denominator) <= 2e-16L &&
            fabsl((long double)candidates[1].relative_candidate_probability -
                  expl(-2.5L - maximum) / denominator) <= 2e-16L &&
            fabsl((long double)candidates[2].relative_candidate_probability -
                  1.0L / denominator) <= 2e-16L,
        "relative finite-population normalization matches an independent oracle");
    printf("decision-readout-distribution sum=%.21Lg max_abs=%.21Lg "
           "tolerance=%.1Le calibrated=false\n",
           observed_sum, fabsl(observed_sum - 1.0L), tolerance);
    candidates[1].candidate_log_likelihood = INFINITY;
    YVEX_TEST_ASSERT(
        yvex_decision_readout_relative_distribution(
            candidates, 3ull, &err) == YVEX_ERR_FORMAT &&
            yvex_decision_readout_relative_distribution(
                NULL, 0ull, &err) == YVEX_ERR_INVALID_ARG,
        "relative normalization refuses malformed and non-finite populations");
    return 0;
}

int yvex_test_decision_readout(void)
{
    if (decision_readout_score_math() != 0) return 1;
    if (decision_readout_distribution() != 0) return 1;
    return 0;
}
