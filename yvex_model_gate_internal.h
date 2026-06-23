/*
 * YVEX - Model gate internals
 */
#ifndef YVEX_MODEL_GATE_INTERNAL_H
#define YVEX_MODEL_GATE_INTERNAL_H

#include <yvex/yvex.h>

int yvex_model_gate_sha256_hex(const unsigned char *data,
                               unsigned long long len,
                               char out_hex[65]);

#endif /* YVEX_MODEL_GATE_INTERNAL_H */
