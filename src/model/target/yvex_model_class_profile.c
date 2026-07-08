/*
 * yvex_model_class_profile.c - model-class profile report boundary.
 *
 * Owner: src/model/target
 * Owns: model-class profile ownership.
 * Does not own: CLI parsing, rendering, tensor payload loading, runtime, generation, benchmark, or release readiness.
 * Invariants: profile facts stay lexical/header-only unless a future row promotes them.
 * Boundary: model-class profiles are not runtime descriptors.
 */
#include "yvex_model_class_profile.h"

typedef int yvex_model_class_profile_file_boundary;
