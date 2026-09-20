/* Provider subprocess authentication policy; credentials stay in the standard provider store. */
#ifndef INCLUDE_YVEX_INTERNAL_PROVIDER_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROVIDER_H_INCLUDED
#include <yvex/source.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Apply only in a forked provider child, never to the host process environment. */
int yvex_provider_child_environment(int anonymous, const char *token,
                                    const char *hf_hub_cache,
                                    const char *hf_xet_cache);
int yvex_provider_capture(yvex_account_capture_options *options,
                           int anonymous, int offline, yvex_error *err);
#ifdef __cplusplus
}
#endif
#endif
