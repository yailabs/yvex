/* Preserve exact Metal refusal on builds without the native Apple Silicon implementation. */
#include <yvex/internal/backend.h>

#if !defined(__APPLE__) || !defined(__aarch64__)
int yvex_backend_open_metal_impl(yvex_backend **out, const char *device,
                                unsigned long long memory_limit_bytes, yvex_error *err)
{
    (void)device;
    (void)memory_limit_bytes;
    *out = NULL;
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "backend.metal.open",
                   "Metal backend requires native macOS arm64 build");
    return YVEX_ERR_UNSUPPORTED;
}
#endif
