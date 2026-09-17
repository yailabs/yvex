/* Execute admitted visual encoders and inject their typed outputs into a language stack. */
#ifndef INCLUDE_YVEX_INTERNAL_MULTIMODAL_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_MULTIMODAL_H_INCLUDED

#include <yvex/core.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/component.h>
#include <yvex/internal/vision_program.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_backend_vision_request {
    const yvex_vision_request *request;
    const yvex_component_encoded_weight *weights;
    unsigned long long weight_count;
    const char *residency_identity;
    unsigned long long resident_bytes;
} yvex_backend_vision_request;

#ifdef __cplusplus
}
#endif
#endif
