/*
 * YVEX - Model reference resolver internals
 */
#ifndef YVEX_MODEL_REF_INTERNAL_H
#define YVEX_MODEL_REF_INTERNAL_H

#include <yvex/model_ref.h>
#include <yvex/model_registry.h>

char *yvex_model_ref_strdup(const char *s);
int yvex_model_ref_copy_from_entry(yvex_model_ref *out,
                                   const char *input,
                                   const yvex_model_registry_entry *entry,
                                   yvex_error *err);

#endif /* YVEX_MODEL_REF_INTERNAL_H */
