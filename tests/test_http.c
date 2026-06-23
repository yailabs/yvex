/*
 * YVEX - HTTP tests
 *
 *
 * Purpose:
 *   Proves the server shell HTTP parser and response builder handle the small yvexd
 *   status endpoint surface.
 */
#include <string.h>

#include "yvex_server_private.h"
#include "test.h"

static int test_parse_request(void)
{
    yvex_http_request request;
    yvex_error err;
    int rc;

    rc = yvex_http_parse_request("GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n", &request, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "parse health");
    YVEX_TEST_ASSERT_STREQ(request.method, "GET", "method");
    YVEX_TEST_ASSERT_STREQ(request.path, "/health", "path");

    rc = yvex_http_parse_request("GET /metrics HTTP/1.1\r\n\r\n", &request, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "parse metrics");
    YVEX_TEST_ASSERT_STREQ(request.path, "/metrics", "metrics path");

    rc = yvex_http_parse_request("POST /v1/completions HTTP/1.1\r\n\r\n", &request, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "parse post generation endpoint");
    YVEX_TEST_ASSERT_STREQ(request.method, "POST", "post method");

    rc = yvex_http_parse_request("GET /health HTTP/1.1", &request, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "incomplete request rejected");
    return 0;
}

static int test_response_format(void)
{
    yvex_http_response response;
    yvex_error err;
    char buf[YVEX_HTTP_RESPONSE_CAP];
    int rc;

    memset(&response, 0, sizeof(response));
    response.status_code = 200;
    response.reason = yvex_http_status_reason(200);
    strcpy(response.body, "{ \"ok\": true }\n");

    rc = yvex_http_response_format(buf, sizeof(buf), &response, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "format response");
    YVEX_TEST_ASSERT(strstr(buf, "HTTP/1.1 200 OK") != NULL, "status line");
    YVEX_TEST_ASSERT(strstr(buf, "Content-Type: application/json") != NULL, "content type");
    YVEX_TEST_ASSERT(strstr(buf, "{ \"ok\": true }") != NULL, "body");
    return 0;
}

int main(void)
{
    if (test_parse_request() != 0) return 1;
    if (test_response_format() != 0) return 1;
    return 0;
}
