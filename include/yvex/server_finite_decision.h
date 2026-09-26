/* Typed finite-decision execution on an independently resident host engine. */
#ifndef INCLUDE_YVEX_SERVER_FINITE_DECISION_H_INCLUDED
#define INCLUDE_YVEX_SERVER_FINITE_DECISION_H_INCLUDED

#include <yvex/finite_decision.h>
#include <yvex/server.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Local C producer only; it does not use the OpenAI-compatible transport. */
int yvex_server_finite_decision_execute(yvex_server *server, const char *alias,
    const yvex_finite_decision_request *request,
    yvex_finite_decision_result *result, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
