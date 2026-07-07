/*
 * yvex_kv_private.h - private KV report dependencies.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   narrow private declarations needed by KV report construction.
 *
 * Does not own:
 *   adapter declarations, renderer declarations, input grammar, stdout or
 *   stderr output, attention execution, decode, generation, eval, benchmark,
 *   or release decisions.
 *
 * Invariants:
 *   this header exposes only declarations required by KV-owned report facts.
 *
 * Boundary:
 *   private declarations are not runtime KV support.
 */
#ifndef YVEX_KV_PRIVATE_H
#define YVEX_KV_PRIVATE_H

#include "yvex_kv_report.h"

void fill_kv_demo_values(float *values,
                         unsigned long long value_count,
                         unsigned long long position);
unsigned long long checksum_kv_values(const float *values,
                                      unsigned long long value_count);

#endif
