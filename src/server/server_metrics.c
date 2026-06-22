/*
 * YVEX - Server metrics counters
 *
 * File: src/server/server_metrics.c
 * Layer: server implementation
 *
 * Purpose:
 *   Updates the small K0 yvexd request/error counters exposed by /metrics.
 */
#include "server_internal.h"

void yvex_server_record_response(yvex_server *server, int status_code)
{
    if (!server) {
        return;
    }
    server->request_count += 1u;
    if (status_code >= 400) {
        server->error_count += 1u;
    }
}
