/* Owner: src/model/target
 * Owns: explicit local sidecar writer entry points.
 * Does not own: CLI operator streams, command dispatch, rendering, runtime execution, generation, eval, benchmark,
 *   or release decisions.
 * Invariants: sidecar writer APIs write explicit local files only and never process operator streams.
 * Boundary: sidecar writer availability does not create artifact emission capability, runtime support, generation
 *   support, benchmark evidence, or release readiness.
 * Purpose: publish explicit model-target sidecars through atomic bounded writes.
 * Inputs: typed report facts and caller-selected sidecar paths.
 * Effects: creates temporary files and atomically replaces only owned destinations.
 * Failure: serialization or I/O failure removes owned temporary state. */
#include <yvex/internal/model_target.h>
#include <yvex/internal/core.h>
#include <yvex/internal/io.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Purpose: construct bounded sidecar open tmp state from admitted inputs.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
static int sidecar_open_tmp(const char *path, char *tmp, size_t tmp_cap, FILE **out)
{
    yvex_error err;
    int n;

    if (!path || !tmp || tmp_cap == 0 || !out) return 0;
    *out = NULL;
    yvex_error_clear(&err);
    if (yvex_core_mkdir_parent(path, "model_target.sidecar", &err) != YVEX_OK)
        return 0;
    n = snprintf(tmp, tmp_cap, "%s.tmp", path);
    if (n < 0 || (size_t)n >= tmp_cap) return 0;
    *out = fopen(tmp, "wb");
    return *out != NULL;
}

/* Purpose: release owned sidecar close tmp resources in dependency order.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
static int sidecar_close_tmp(FILE *fp, const char *tmp, const char *path)
{
    if (!fp || !tmp || !path) return 0;
    if (fclose(fp) != 0) {
        remove(tmp);
        return 0;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}

/* Purpose: publish write tensor map sidecar through the bounded output boundary.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_write_tensor_map_sidecar(const char *path,
                                               const char *target_id,
                                               const char *family,
                                               const char *status,
                                               const char *coverage)
{
    char tmp[1024];
    FILE *fp;

    if (!path || !path[0]) return 1;
    if (!sidecar_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.source.tensor_map.v1\",\n");
    fprintf(fp, "  \"row\": \"MODELS.SOURCE.MAP.HANDOFF.0\",\n");
    fprintf(fp, "  \"target_id\": ");
    yvex_file_json_write_string(fp, target_id);
    fprintf(fp, ",\n  \"family\": ");
    yvex_file_json_write_string(fp, family);
    fprintf(fp, ",\n  \"tensor_map_status\": ");
    yvex_file_json_write_string(fp, status);
    fprintf(fp, ",\n  \"required_role_coverage_status\": ");
    yvex_file_json_write_string(fp, coverage);
    fprintf(fp, "\n}\n");
    return sidecar_close_tmp(fp, tmp, path);
}

/* Purpose: publish write output head sidecar through the bounded output boundary.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_write_output_head_sidecar(const char *path,
                                                const char *target_id,
                                                const char *family,
                                                const char *status)
{
    char tmp[1024];
    FILE *fp;

    if (!path || !path[0]) return 1;
    if (!sidecar_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.source.output_head_map.v1\",\n");
    fprintf(fp, "  \"row\": \"MODELS.SOURCE.MAP.HANDOFF.0\",\n");
    fprintf(fp, "  \"target_id\": ");
    yvex_file_json_write_string(fp, target_id);
    fprintf(fp, ",\n  \"family\": ");
    yvex_file_json_write_string(fp, family);
    fprintf(fp, ",\n  \"output_head_map_status\": ");
    yvex_file_json_write_string(fp, status);
    fprintf(fp, ",\n  \"output_head_status\": ");
    yvex_file_json_write_string(fp,
                        strcmp(status, "output-head-missing") == 0
                            ? "missing"
                            : "present");
    fprintf(fp, "\n}\n");
    return sidecar_close_tmp(fp, tmp, path);
}

/* Purpose: publish write tokenizer sidecar through the bounded output boundary.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_write_tokenizer_sidecar(const char *path,
                                              const char *target_id,
                                              const char *family,
                                              const char *status)
{
    char tmp[1024];
    FILE *fp;

    if (!path || !path[0]) return 1;
    if (!sidecar_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema_version\": \"yvex.source.tokenizer_map.v1\",\n");
    fprintf(fp, "  \"row\": \"V010.MAP.7\",\n");
    fprintf(fp, "  \"target_id\": ");
    yvex_file_json_write_string(fp, target_id);
    fprintf(fp, ",\n  \"family\": ");
    yvex_file_json_write_string(fp, family);
    fprintf(fp, ",\n  \"tokenizer_map_status\": ");
    yvex_file_json_write_string(fp, status);
    fprintf(fp, ",\n  \"status\": ");
    yvex_file_json_write_string(fp, status);
    fprintf(fp, "\n}\n");
    return sidecar_close_tmp(fp, tmp, path);
}
