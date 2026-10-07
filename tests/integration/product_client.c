/* Measurement adapter, not a product command or independent wire implementation.
 * Use the same typed C protocol owner as Rust chat. Own only a fresh bench session. */
#include <yvex/server.h>
#include <yvex/provider.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static int positive_integer(const char *text, unsigned long long *value)
{
    char *end = NULL;
    if (!text || !text[0] || text[0] == '-' || text[0] == '+') return 0;
    errno = 0;
    *value = strtoull(text, &end, 10);
    return !errno && end && !*end && *value != 0;
}

static double now(void)
{
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value)) abort();
    return (double)value.tv_sec + (double)value.tv_nsec / 1e9;
}

static int exchange(const char *socket, yvex_client_request *request,
                    yvex_client_message *reply, double *started, yvex_error *error)
{
    yvex_client *client = NULL;
    int status;
    *started = now();
    status = yvex_client_connect(&client, socket, error);
    if (!status) status = yvex_client_timeout_set(client, 1800000ull, error);
    if (!status) status = yvex_client_send(client, request, error);
    while (!status) {
        status = yvex_client_receive(client, reply, error);
        if (status) break;
        if (reply->request_number != request->request_number) {
            status = YVEX_ERR_STATE;
            break;
        }
        if (reply->status) { status = reply->status; break; }
        if (reply->kind == YVEX_CLIENT_MESSAGE_TURN_STARTED) {
            printf("{\"kind\":\"started\",\"request\":%llu,\"seconds\":%.9f}\n",
                   reply->request_number, now() - *started);
            fflush(stdout);
        }
        if (reply->kind == YVEX_CLIENT_MESSAGE_FRAGMENT) {
            if (reply->byte_count > sizeof(reply->bytes)) { status = YVEX_ERR_BOUNDS; break; }
            printf("{\"kind\":\"fragment\",\"request\":%llu,\"seconds\":%.9f,\"channel\":%u,\"hex\":\"",
                   reply->request_number, now() - *started, (unsigned int)reply->stream_channel);
            for (unsigned long long i = 0; i < reply->byte_count; ++i) printf("%02x", reply->bytes[i]);
            puts("\"}");
            fflush(stdout);
        }
        if (reply->kind == YVEX_CLIENT_MESSAGE_TURN_COMPLETE ||
            reply->kind == YVEX_CLIENT_MESSAGE_SESSION || reply->kind == YVEX_CLIENT_MESSAGE_ACK) break;
    }
    yvex_client_close(&client);
    return status;
}

static void summary(const yvex_client_message *r, double elapsed)
{
    printf("{\"kind\":\"turn\",\"request\":%llu,\"client_complete_seconds\":%.9f,"
           "\"prompt_tokens\":%llu,\"reused_tokens\":%llu,\"prefill_tokens\":%llu,"
           "\"generated_tokens\":%llu,\"initial_position\":%llu,\"final_position\":%llu,"
           "\"prefill_seconds\":%.9f,\"prefill_rate\":%.9f,\"server_first_token_seconds\":%.9f,"
           "\"decode_seconds\":%.9f,\"decode_rate\":%.9f,\"publication_seconds\":%.9f,"
           "\"publication_timing_available\":%d,\"reasoning_tokens\":%llu,\"final_tokens\":%llu,"
           "\"first_reasoning_seconds\":%.9f,\"first_final_seconds\":%.9f,"
           "\"reasoning_seconds\":%.9f,\"final_seconds\":%.9f,\"total_completion_seconds\":%.9f,"
           "\"draft_cycles\":%llu,\"draft_forwards\":%llu,\"proposed_tokens\":%llu,"
           "\"selected_verification_tokens\":%llu,\"target_verifications\":%llu,"
           "\"accepted_tokens\":%llu,\"rejected_tokens\":%llu,\"discarded_tokens\":%llu,"
           "\"correction_or_bonus_tokens\":%llu,\"maximum_accepted_prefix\":%llu,"
           "\"mean_accepted_prefix\":%.9f,\"draft_seconds\":%.9f,\"verification_seconds\":%.9f,"
           "\"post_first_decode_scope\":%u,\"post_first_decode_available\":%llu,"
           "\"post_first_decode_units\":%llu,\"post_first_decode_ns\":%llu,\"post_first_decode_rate\":%.9f,"
           "\"commit_seconds\":%.9f,\"stop_reason\":%u,\"strategy\":%u,"
           "\"turn_identity\":\"%s\",\"token_identity\":\"%s\"}\n",
           r->request_number, elapsed, r->prompt_tokens, r->reused_tokens, r->prefill_tokens,
           r->generated_tokens, r->initial_position, r->final_position, r->prefill_seconds,
           r->prefill_rate, r->first_token_seconds, r->decode_seconds, r->decode_rate,
           r->publication_seconds, r->publication_timing_available, r->reasoning_tokens,
           r->final_tokens, r->first_reasoning_seconds, r->first_final_seconds,
           r->reasoning_seconds, r->final_seconds, r->total_completion_seconds,
           r->draft_cycle_count, r->draft_forward_count, r->proposed_tokens,
           r->selected_verification_tokens, r->target_verification_count, r->accepted_draft_tokens,
           r->rejected_draft_tokens, r->discarded_draft_tokens, r->target_correction_or_bonus_tokens,
           r->maximum_accepted_prefix, r->mean_accepted_prefix, r->draft_seconds,
           r->verification_seconds, (unsigned int)r->measurement.scope, r->measurement.available,
           r->measurement.completed_units, r->measurement.duration_ns, r->measurement.cumulative_rate,
           r->speculative_commit_seconds, r->stop_reason,
           (unsigned int)r->execution_strategy, r->turn_identity, r->generated_token_identity);
}

int main(int argc, char **argv)
{
    yvex_client_request request = {0};
    yvex_client_message *reply = calloc(1, sizeof(*reply));
    yvex_provider_request defaults;
    yvex_error error = {0};
    double started = 0;
    int status = YVEX_ERR_BOUNDS, created = 0;
    if (argc < 9 || !reply || strncmp(argv[4], "bench-", 6)) goto done;
    if (strlen(argv[2]) >= sizeof(request.model_alias) ||
        strlen(argv[4]) >= sizeof(request.session_name)) goto done;
    yvex_provider_request_default(&defaults);
    request.schema_version = YVEX_LOCAL_PROTOCOL_VERSION;
    request.request_number = 1;
    if (!positive_integer(argv[3], &request.engine_generation)) goto done;
    strcpy(request.model_alias, argv[2]); strcpy(request.session_name, argv[4]);
    if (!positive_integer(argv[6], &request.maximum_new_tokens)) goto done;
    request.stochastic = defaults.sampling.stochastic;
    request.seed_present = defaults.sampling.seed_present; request.seed = defaults.sampling.seed;
    request.temperature = defaults.sampling.temperature; request.top_k = defaults.sampling.top_k;
    request.top_p = defaults.sampling.top_p; request.min_p = defaults.sampling.min_p;
    request.typical_p = defaults.sampling.typical_p;
    if (!strcmp(argv[7], "greedy")) { request.stochastic = 0; request.temperature = 0; }
    else if (strcmp(argv[7], "product")) goto done;
    if (!strcmp(argv[5], "none")) request.reasoning_policy = YVEX_REASONING_DISABLED;
    else if (!strcmp(argv[5], "high")) request.reasoning_policy = YVEX_REASONING_ENABLED;
    else if (!strcmp(argv[5], "maximum")) request.reasoning_policy = YVEX_REASONING_MAXIMUM;
    else goto done;
    request.operation = YVEX_CLIENT_OP_SESSION_NEW;
    status = exchange(argv[1], &request, reply, &started, &error);
    if (status) goto done;
    if (reply->kind != YVEX_CLIENT_MESSAGE_SESSION) { status = YVEX_ERR_STATE; goto done; }
    created = 1;
    for (int index = 8; index < argc; ++index) {
        unsigned char *prompt = malloc(1024u * 1024u + 1u);
        FILE *file = fopen(argv[index], "rb");
        size_t length;
        if (!prompt || !file) { free(prompt); if (file) fclose(file); status = YVEX_ERR_IO; break; }
        length = fread(prompt, 1, 1024u * 1024u + 1u, file);
        status = ferror(file) || length > 1024u * 1024u ? YVEX_ERR_BOUNDS : YVEX_OK;
        fclose(file);
        if (!status) {
            request.prompt = prompt; request.prompt_bytes = length;
            request.operation = YVEX_CLIENT_OP_GENERATION_TURN; ++request.request_number;
            status = exchange(argv[1], &request, reply, &started, &error);
            if (!status && reply->kind != YVEX_CLIENT_MESSAGE_TURN_COMPLETE) status = YVEX_ERR_STATE;
            if (!status) summary(reply, now() - started);
        }
        free(prompt); request.prompt = NULL; request.prompt_bytes = 0;
        if (status) break;
    }
    /* Do not blindly close or retry indeterminate work. The external owner must
     * reconcile this uniquely named session on failure. */
    if (!status) {
        request.operation = YVEX_CLIENT_OP_SESSION_CLOSE; ++request.request_number;
        status = exchange(argv[1], &request, reply, &started, &error);
        if (!status) created = 0;
    }
done:
    fprintf(stderr, "native measurement status=%d session_retained=%d reason=%s\n",
            status, created, yvex_error_message(&error));
    free(reply);
    return status ? 1 : 0;
}
