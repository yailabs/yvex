/* Source authentication, sealed preparation plans and immutable binding recovery.
 * This internal lease has no command parser, renderer, engine or session ownership. */
#ifndef INCLUDE_YVEX_INTERNAL_MODEL_PREPARATION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_MODEL_PREPARATION_H_INCLUDED

#include <yvex/catalog.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_model_preparation yvex_model_preparation;
/* The selected-tensor conversion recipe is diagnostic, not a generation
 * profile. Process-lifetime strings; unknown targets clear out and refuse. */
typedef struct {
    const char *target, *family, *architecture, *tensor, *qtype;
    const char *artifact_leaf, *plan_leaf, *repository, *revision;
    const char *artifact_tensor, *gate_label;
    unsigned long long gate_dims[2], gate_bytes;
    const char *reason;
    int implemented;
} yvex_model_preparation_recipe;
int yvex_model_preparation_recipe_get(const char *target,
    yvex_model_preparation_recipe *out, yvex_error *err);
typedef struct {
    const char *models_root, *registry_path, *quant, *imatrix;
    int dry_run;
} yvex_model_preparation_request;
typedef struct {
    const char *source, *revision, *target, *quant, *backend, *strategy;
    const char *family, *model, *models_root, *registry, *manifest, *plan;
    const char *artifact, *binding, *profile;
    int rebind;
} yvex_model_preparation_view;

/* The immutable catalog and request strings must survive close. Open leaves
 * *out NULL on failure. A view survives until the next mutating method/close;
 * copy facts before advancing. Callers hold the source acquisition lock through
 * catalog selection, preparation and profile publication. Dry contexts refuse
 * mutation. Verify authenticates exact source identity before later methods. */
int yvex_model_preparation_open(yvex_model_preparation **out, const yvex_model_library *library,
    unsigned long long index, const yvex_model_preparation_request *request, yvex_error *err);
void yvex_model_preparation_close(yvex_model_preparation *context);
int yvex_model_preparation_ready_verify(const yvex_model_library *library,
    unsigned long long index, const yvex_model_preparation_request *request, yvex_error *err);
int yvex_model_preparation_view_get(const yvex_model_preparation *context,
                                   yvex_model_preparation_view *out, yvex_error *err);
int yvex_model_preparation_verify(yvex_model_preparation *context, yvex_error *err);
/* Seal the caller-produced physical plan under its authenticated variant
 * identity; never replace a conflicting immutable plan. Existing artifacts
 * and compiled bindings remain separately authenticated, not inferred from a
 * successful compiler invocation or a populated catalog. */
int yvex_model_preparation_store_plan(yvex_model_preparation *context, yvex_error *err);
int yvex_model_preparation_cached(yvex_model_preparation *context, const yvex_model_library *library,
                                 unsigned long long index, int *cached, yvex_error *err);
int yvex_model_preparation_artifact_verify(yvex_model_preparation *context, yvex_error *err);
int yvex_model_preparation_binding_publish(yvex_model_preparation *context,
                                          int *published, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif /* INCLUDE_YVEX_INTERNAL_MODEL_PREPARATION_H_INCLUDED */
