/*
 * yvex_gguf_name_map.c - emitted GGUF tensor name plan facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   runtime-role to emitted GGUF tensor-name plan boundary and missing-name
 *   blockers.
 *
 * Does not own:
 *   source native role mapping, layout/range planning, writer bytes,
 *   materialization, graph binding, or generation.
 *
 * Invariants:
 *   emitted names are planned facts only until V010.MAP.GGUF.NAMES.0.
 *
 * Boundary:
 *   name-map facts do not imply byte layout or artifact emission.
 */
#include "yvex_gguf_private.h"

/* Contract: refuses emitted-name readiness before the GGUF name-map row. */
int yvex_gguf_name_map_role_supported(const char *role, const char **reason)
{
    if (!role || !role[0]) {
        if (reason) *reason = "missing runtime role for GGUF emitted name";
        return 0;
    }
    if (reason) *reason = "GGUF emitted-name map is future-owned by V010.MAP.GGUF.NAMES.0";
    return 0;
}
