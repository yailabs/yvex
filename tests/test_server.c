/*
 * YVEX - server tests
 *
 *
 * Purpose:
 *   Proves the server shell server shell creates summaries and routes health, metrics,
 *   model catalog, and unsupported generation endpoints without execution.
 */
#include <string.h>

#include "yvex_internal.h"
#include "test.h"

static int test_server_summary(void)
{
    yvex_server *server = NULL;
    yvex_server_options options;
    yvex_server_summary summary;
    yvex_error err;
    int rc;

    rc = yvex_server_create(&server, NULL, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "default server create");
    rc = yvex_server_get_summary(server, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "default summary");
    YVEX_TEST_ASSERT(summary.status == YVEX_SERVER_STATUS_CREATED, "created status");
    YVEX_TEST_ASSERT_STREQ(summary.engine_status, "not_loaded", "engine not loaded");
    YVEX_TEST_ASSERT(summary.generation_available == 0, "generation unavailable");
    yvex_server_close(server);

    memset(&options, 0, sizeof(options));
    options.host = "127.0.0.1";
    options.port = 18080;
    options.model_path = "tests/fixtures/gguf/valid-tokenizer-simple.gguf";
    options.backend_name = "cpu";
    options.load_engine = 1;
    rc = yvex_server_create(&server, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "server create with model");
    rc = yvex_server_get_summary(server, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "model summary");
    YVEX_TEST_ASSERT_STREQ(summary.engine_status, "partial", "engine partial");
    YVEX_TEST_ASSERT_STREQ(summary.backend_status, "ready", "backend ready");
    yvex_server_close(server);
    return 0;
}

static int test_routes(void)
{
    yvex_server *server = NULL;
    yvex_server_options options;
    yvex_http_request request;
    yvex_http_response response;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.host = "127.0.0.1";
    options.port = 18081;
    options.model_path = "tests/fixtures/gguf/valid-tokenizer-simple.gguf";
    options.backend_name = "cpu";
    options.load_engine = 1;
    rc = yvex_server_create(&server, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "server create for routes");

    strcpy(request.method, "GET");
    strcpy(request.path, "/health");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "health route");
    YVEX_TEST_ASSERT(response.status_code == 200, "health status");
    YVEX_TEST_ASSERT(strstr(response.body, "\"schema\": \"yvex.health.v1\"") != NULL, "health schema");

    strcpy(request.path, "/metrics");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "metrics route");
    YVEX_TEST_ASSERT(strstr(response.body, "\"schema\": \"yvex.server_metrics.v1\"") != NULL, "metrics schema");

    strcpy(request.path, "/v1/models");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "models route");
    YVEX_TEST_ASSERT(strstr(response.body, "\"schema\": \"yvex.models.v1\"") != NULL, "models schema");
    YVEX_TEST_ASSERT(strstr(response.body, "\"inference\": \"not_implemented\"") != NULL, "inference not implemented");

    strcpy(request.method, "POST");
    strcpy(request.path, "/v1/completions");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "unsupported generation route");
    YVEX_TEST_ASSERT(response.status_code == 501, "unsupported status");
    YVEX_TEST_ASSERT(strstr(response.body, "generation endpoints are not implemented in server shell") != NULL,
                     "unsupported message");

    yvex_server_close(server);
    return 0;
}

int main(void)
{
    if (test_server_summary() != 0) return 1;
    if (test_routes() != 0) return 1;
    return 0;
}
