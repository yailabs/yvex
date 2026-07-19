/* Owner: io.internal (io).
 * Owns: checked file serialization primitives.
 * Does not own: domain policy, rendering, or publication admission.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: shared internal file-writing utility.
 * Purpose: provide the canonical shared internal file-writing utility contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_IO_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_IO_H_INCLUDED

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writer contract. */
void yvex_file_json_write_string(FILE *fp, const char *s);
void yvex_file_json_write_field(FILE *fp,
                                const char *indent,
                                const char *name,
                                const char *value,
                                int comma);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_IO_H_INCLUDED */
