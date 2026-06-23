/*
 * YVEX - metrics tests
 *
 * File: tests/test_metrics.c
 * Layer: test
 *
 * Purpose:
 *   Proves the observability layer metrics collector records implemented runtime shell phases
 *   and counters without generated-token performance claims.
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

static int test_metrics_basic(void)
{
    yvex_metrics *metrics = NULL;
    yvex_metric_counters counters;
    yvex_metric_phase_summary phase;
    yvex_error err;
    unsigned long long token;
    int rc;

    rc = yvex_metrics_create(&metrics, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "metrics create");
    YVEX_TEST_ASSERT_STREQ(yvex_metric_phase_name(YVEX_METRIC_PHASE_TOKENIZE), "tokenize", "phase name");

    rc = yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOKENIZE, &token, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "phase begin");
    rc = yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOKENIZE, token, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "phase end");
    rc = yvex_metrics_get_phase(metrics, YVEX_METRIC_PHASE_TOKENIZE, &phase, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "phase summary");
    YVEX_TEST_ASSERT(phase.count == 1, "phase count");

    rc = yvex_metrics_add_prompt_tokens(metrics, 3, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "prompt tokens");
    rc = yvex_metrics_add_accepted_tokens(metrics, 3, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "accepted tokens");
    rc = yvex_metrics_add_rejected_tokens(metrics, 1, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "rejected tokens");
    rc = yvex_metrics_add_chat_turn(metrics, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "chat turn");
    rc = yvex_metrics_set_model_bytes(metrics, 128, 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "model bytes");

    rc = yvex_metrics_get_counters(metrics, &counters, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "counters");
    YVEX_TEST_ASSERT(counters.prompt_tokens == 3, "prompt counter");
    YVEX_TEST_ASSERT(counters.accepted_tokens == 3, "accepted counter");
    YVEX_TEST_ASSERT(counters.rejected_tokens == 1, "rejected counter");
    YVEX_TEST_ASSERT(counters.chat_turns == 1, "chat counter");
    YVEX_TEST_ASSERT(counters.known_tensor_bytes == 128, "known bytes");

    yvex_metrics_reset(metrics);
    rc = yvex_metrics_get_counters(metrics, &counters, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "counters after reset");
    YVEX_TEST_ASSERT(counters.accepted_tokens == 0, "accepted reset");

    rc = yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOKENIZE, 999, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE, "end without begin fails");

    yvex_metrics_close(metrics);
    return 0;
}

int main(void)
{
    if (test_metrics_basic() != 0) return 1;
    return 0;
}
