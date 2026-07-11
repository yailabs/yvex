/*
 * yvex_source_verify_internal.h - shared exact-source verification facts.
 *
 * Owner: src/source.
 * Owns: internal fact mutation and common path/identity helpers used by source owners.
 * Does not own: parsing, inventory, manifest writing, rendering, or product claims.
 * Invariants: blockers are unique and bounded; helper failures never imply verification.
 * Boundary: shared verification facts are not a compatibility backend.
 */
#ifndef YVEX_SOURCE_VERIFY_INTERNAL_H
#define YVEX_SOURCE_VERIFY_INTERNAL_H

#include "yvex_source_verify.h"

void yvex_source_verification_add_blocker(yvex_source_verification *out,
                                          const char *reason);
int yvex_source_verification_has_blocker(
    const yvex_source_verification *out,
    const char *reason);
void yvex_source_verification_remove_blocker(yvex_source_verification *out,
                                             const char *reason);
int yvex_source_path_join(char *out,
                          size_t cap,
                          const char *left,
                          const char *right);
int yvex_source_regular_file(const char *path, unsigned long long *size);
int yvex_source_revision_is_commit(const char *text);

#endif
