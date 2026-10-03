/* Bounded discovery of artifacts and source acquisition sidecars. Presence is
 * never payload authentication, a runtime binding or generation qualification. */
#ifndef INCLUDE_YVEX_INTERNAL_ARTIFACT_CATALOG_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_ARTIFACT_CATALOG_H_INCLUDED
#include <yvex/core.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    char target_id[128];
    char family[32];
    char artifact_class[64];
    char artifact_status[32];
    char source_status[32];
    char prepare_status[32];
    char top_blocker[64];
    char detail[256];
    char tensor_map_status[32];
    char output_head_map_status[32];
    char tokenizer_map_status[32];
    unsigned int prepare_blocker_count;
    char path[YVEX_PATH_CAP];
    char expected_path[YVEX_PATH_CAP];
    char display_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char source_manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    char tensor_map_path[YVEX_PATH_CAP];
    char output_head_map_path[YVEX_PATH_CAP];
    char tokenizer_map_path[YVEX_PATH_CAP];
    unsigned long long size_bytes;
    int source_present;
    int tensor_map_present;
    int tensor_map_incomplete;
    int output_head_map_present;
    int output_head_missing;
    int tokenizer_map_present;
    int dynamic_source;
} yvex_artifact_catalog_row;
typedef struct yvex_artifact_catalog yvex_artifact_catalog;
int yvex_artifact_catalog_open(yvex_artifact_catalog **out, const yvex_operator_paths *paths,
    const char *family, yvex_error *err);
void yvex_artifact_catalog_close(yvex_artifact_catalog *catalog);
unsigned int yvex_artifact_catalog_count(const yvex_artifact_catalog *catalog);
/* Borrowed immutable row until close. NULL at or beyond count. */
const yvex_artifact_catalog_row *yvex_artifact_catalog_at(
    const yvex_artifact_catalog *catalog, unsigned int index);
const char *yvex_artifact_catalog_next(const yvex_artifact_catalog_row *row);
#ifdef __cplusplus
}
#endif
#endif /* INCLUDE_YVEX_INTERNAL_ARTIFACT_CATALOG_H_INCLUDED */
