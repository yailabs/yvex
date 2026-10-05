/* Synthetic peer: exercise the real C client/codecs, not model arithmetic. */
#define _POSIX_C_SOURCE 200809L
#include "src/server/private.h"
#include <yvex/internal/finite_producer_wire.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

static int serve(int fd)
{
    yvex_client_request request = {0};
    yvex_client_message reply = {0};
    yvex_finite_producer_request finite = {0};
    yvex_finite_producer_result result = {0};
    unsigned char *prompt = NULL;
    yvex_content_part *content = NULL;
    yvex_provider_request *provider = NULL;
    yvex_error error = {0};
    struct timeval timeout = {.tv_sec = 10};
    int rc;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    rc = yvex_server_protocol_receive(fd, &request, &prompt, &content, &provider, &error);
    if (rc || request.operation != YVEX_CLIENT_OP_HANDSHAKE) goto done;
    reply.kind = YVEX_CLIENT_MESSAGE_ACK;
    reply.schema_version = YVEX_LOCAL_PROTOCOL_VERSION;
    (void)snprintf(reply.reason, sizeof(reply.reason), "protocol-v%u", YVEX_LOCAL_PROTOCOL_VERSION);
    rc = yvex_server_protocol_send(fd, &reply, &error);
    if (rc) goto done;
    free(prompt); free(content); free(provider);
    prompt = NULL; content = NULL; provider = NULL;
    rc = yvex_server_protocol_receive(fd, &request, &prompt, &content, &provider, &error);
    if (rc || request.operation != YVEX_CLIENT_OP_FINITE_DECISION) goto done;
    rc = yvex_finite_producer_request_decode(request.prompt, request.prompt_bytes, &finite, &error);
    if (rc) goto done;
    puts("DISPATCH"); fflush(stdout);
    memset(&reply, 0, sizeof(reply));
    reply.schema_version = YVEX_LOCAL_PROTOCOL_VERSION;
    reply.request_number = request.request_number;
    if (finite.expected_generation != 9u || !strcmp(finite.question, "refuse")) {
        reply.kind = YVEX_CLIENT_MESSAGE_ERROR;
        reply.stream_channel = YVEX_CLIENT_STREAM_ERROR;
        reply.provider_output_kind = YVEX_PROVIDER_OUTPUT_ERROR;
        reply.status = YVEX_ERR_STATE;
        strcpy(reply.reason, "synthetic stale/refusal");
    } else {
        result.schema_version = YVEX_FINITE_PRODUCER_SCHEMA_V1;
        result.score_kind = YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT;
        result.engine_generation = !strcmp(finite.question, "foreign-generation") ? 10u : 9u;
        result.token_count = 17u;
        result.candidate_count = finite.candidate_count;
        result.model_forward_count = 1u;
        result.resident_backbone_count = 1u;
        char *ids[] = {result.source_identity, result.logical_model_identity,
            result.binding_identity, result.tokenizer_identity, result.physical_program_identity,
            result.input_policy_identity, result.input_identity,
            result.candidate_population_identity, result.result_identity};
        for (size_t i = 0u; i < sizeof(ids) / sizeof(ids[0]); ++i) memset(ids[i], 'a', 64u);
        for (unsigned long long i = 0u; i < finite.candidate_count; ++i) {
            strcpy(result.candidates[i].id, finite.candidates[i].id);
            result.candidates[i].raw_score = (double)i;
            result.candidates[i].relative_candidate_probability = 1.0 / (double)finite.candidate_count;
        }
        if (!strcmp(finite.question, "foreign-population")) strcpy(result.candidates[0].id, "foreign");
        size_t bytes = 0u;
        rc = yvex_finite_producer_result_encode(&result, reply.bytes, sizeof(reply.bytes), &bytes, &error);
        if (rc) goto done;
        reply.kind = YVEX_CLIENT_MESSAGE_FINITE_DECISION;
        reply.byte_count = bytes;
        if (!strcmp(finite.question, "delay")) sleep(2);
    }
    rc = yvex_server_protocol_send(fd, &reply, &error);
done:
    if (rc) fprintf(stderr, "peer error=%d reason=%s\n", rc, error.message);
    free(prompt); free(content); free(provider);
    return rc;
}

int main(int argc, char **argv)
{
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    if (argc != 2 || strlen(argv[1]) >= sizeof(address.sun_path)) return 2;
    strcpy(address.sun_path, argv[1]);
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0 || bind(listener, (struct sockaddr *)&address, sizeof(address)) ||
        chmod(argv[1], 0600) || listen(listener, 8)) return 2;
    signal(SIGPIPE, SIG_IGN);
    puts("READY"); fflush(stdout);
    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) return 2;
        (void)serve(fd);
        close(fd);
        puts("CLOSED"); fflush(stdout);
    }
}
