/* Registry admission is a typed server service, not a CLI callback implementation. */
#ifndef INCLUDE_YVEX_INTERNAL_SERVER_LOADER_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_SERVER_LOADER_H_INCLUDED
#include <yvex/server.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_server_registry_loader yvex_server_registry_loader;
/* Own the loader until all server work is finished and the server is closed.
 * Creation is failure-atomic; close clears the caller's handle. */
int yvex_server_registry_loader_create(yvex_server_registry_loader **out,
                                       yvex_server_trace_level trace_level,
                                       yvex_error *err);
void yvex_server_registry_loader_close(yvex_server_registry_loader **loader);
/* Implements yvex_server_model_loader; server workers may call concurrently.
 * The registry is serialized, copied facts are used after releasing its lock,
 * and engine admission remains in the existing server/runtime owners. */
int yvex_server_registry_model_load(void *context, yvex_server *server,
                                    const char *alias,
                                    unsigned long long requested_context_capacity,
                                    yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
