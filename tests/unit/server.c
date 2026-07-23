/*
 * YVEX - server tests
 *
 *
 * Purpose:
 *   Proves the server shell creates unloaded summaries, rejects the retired
 *   diagnostic engine path, and routes non-execution endpoints.
 */
#include <string.h>

#include <yvex/server.h>
#include "tests/test.h"

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
    server = NULL;
    rc = yvex_server_create(&server, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "server rejects retired engine loading");
    YVEX_TEST_ASSERT(server == NULL, "failed server create publishes no handle");
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
    options.model_path = NULL;
    options.backend_name = NULL;
    options.load_engine = 0;
    rc = yvex_server_create(&server, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "server create for routes");

    strcpy(request.method, "GET");
    strcpy(request.path, "/health");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "health route");
    YVEX_TEST_ASSERT(response.status_code == 200, "health status");
    YVEX_TEST_ASSERT(strstr(response.body, "\"schema\": \"api.health.v1\"") != NULL, "health schema");

    strcpy(request.path, "/metrics");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "metrics route");
    YVEX_TEST_ASSERT(strstr(response.body, "\"schema\": \"yvex.server_metrics.v1\"") != NULL, "metrics schema");

    strcpy(request.path, "/v1/models");
    rc = yvex_server_route(server, &request, &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "models route");
    YVEX_TEST_ASSERT(strstr(response.body, "\"schema\": \"yvex.models.v1\"") != NULL, "models schema");
    YVEX_TEST_ASSERT(strstr(response.body, "\"generation_available\": false") != NULL,
                     "generation remains unavailable");
    YVEX_TEST_ASSERT(strstr(response.body, "\"data\": []") != NULL,
                     "retired diagnostic engine publishes no model");

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

int yvex_test_server(void)
{
    if (test_server_summary() != 0) return 1;
    if (test_routes() != 0) return 1;
    return 0;
}
