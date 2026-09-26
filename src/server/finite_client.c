/* Typed finite-decision producer client over the existing private local host. */
#include <yvex/finite_decision_producer.h>
#include <yvex/internal/finite_producer_wire.h>
#include <yvex/server.h>
#include <string.h>

static int client_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "server.finite-client", reason);
    return code;
}

int yvex_finite_producer_execute_local(const char *socket_path,
    const yvex_finite_producer_request *request,
    yvex_finite_producer_result *result, yvex_error *err)
{
    unsigned char payload[4096];
    size_t payload_bytes = 0u;
    yvex_client *client = NULL;
    yvex_client_message reply = {0};
    if (result) memset(result, 0, sizeof(*result));
    if (!request || !result)
        return client_refuse(err, YVEX_ERR_INVALID_ARG, "typed request and result required");
    int rc = yvex_finite_producer_request_encode(request, payload,
        sizeof(payload), &payload_bytes, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_client_connect(&client, socket_path, err);
    if (rc != YVEX_OK) return rc;
    yvex_client_request outbound = {.schema_version = YVEX_LOCAL_PROTOCOL_VERSION,
        .operation = YVEX_CLIENT_OP_FINITE_DECISION, .request_number = 1u,
        .engine_generation = request->expected_generation,
        .prompt = payload, .prompt_bytes = payload_bytes};
    memcpy(outbound.model_alias, request->model_alias, sizeof(outbound.model_alias));
    rc = yvex_client_send(client, &outbound, err);
    if (rc == YVEX_OK) rc = yvex_client_receive(client, &reply, err);
    yvex_client_close(&client);
    if (rc != YVEX_OK) return rc;
    if (reply.kind == YVEX_CLIENT_MESSAGE_ERROR) {
        yvex_error_set(err, reply.status, "server.finite-client.remote", reply.reason);
        return reply.status;
    }
    if (reply.kind != YVEX_CLIENT_MESSAGE_FINITE_DECISION ||
        reply.status != YVEX_OK || reply.request_number != 1u)
        return client_refuse(err, YVEX_ERR_FORMAT, "unexpected finite producer response");
    rc = yvex_finite_producer_result_decode(reply.bytes,
        (size_t)reply.byte_count, result, err);
    if (rc != YVEX_OK) {
        memset(result, 0, sizeof(*result));
        return rc;
    }
    if (result->engine_generation != request->expected_generation ||
        result->candidate_count != request->candidate_count) {
        memset(result, 0, sizeof(*result));
        return client_refuse(err, YVEX_ERR_STATE, "foreign or stale finite producer result");
    }
    for (unsigned long long i = 0u; i < result->candidate_count; ++i)
        if (strcmp(result->candidates[i].id, request->candidates[i].id)) {
            memset(result, 0, sizeof(*result));
            return client_refuse(err, YVEX_ERR_FORMAT, "candidate population changed in transit");
        }
    yvex_error_clear(err);
    return YVEX_OK;
}
