#ifndef YVEX_ERROR_H
#define YVEX_ERROR_H

#include <yvex/status.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    yvex_status code;
    char where[96];
    char message[256];
} yvex_error;

void yvex_error_clear(yvex_error *err);
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *message);
void yvex_error_setf(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...);
int yvex_error_is_set(const yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif
