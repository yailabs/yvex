/*
 * YVEX - Safetensors header reader
 *
 * File: yvex_safetensors.c
 * Layer: tool-plane implementation
 */
#include "yvex_native_weights_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static unsigned long long st_le64(const unsigned char b[8])
{
    return ((unsigned long long)b[0]) |
           ((unsigned long long)b[1] << 8) |
           ((unsigned long long)b[2] << 16) |
           ((unsigned long long)b[3] << 24) |
           ((unsigned long long)b[4] << 32) |
           ((unsigned long long)b[5] << 40) |
           ((unsigned long long)b[6] << 48) |
           ((unsigned long long)b[7] << 56);
}

int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err)
{
    FILE *fp;
    struct stat st;
    unsigned char len_bytes[8];
    unsigned long long header_len;
    unsigned long long file_size;
    unsigned long long payload_bytes;
    char *json;
    int rc;

    if (!abs_path || !shard_path || !table) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "safetensors_header", "path, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (stat(abs_path, &st) != 0 || st.st_size < 8) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "short safetensors file: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    file_size = (unsigned long long)st.st_size;
    fp = fopen(abs_path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "safetensors_header", "cannot open safetensors file: %s", shard_path);
        return YVEX_ERR_IO;
    }
    if (fread(len_bytes, 1, 8, fp) != 8) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "cannot read safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    header_len = st_le64(len_bytes);
    if (header_len == 0 || header_len > file_size - 8 || header_len > 64ull * 1024ull * 1024ull) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "invalid safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "safetensors_header", "header allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(json, 1, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "cannot read safetensors header: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    fclose(fp);
    json[header_len] = '\0';
    payload_bytes = file_size - 8 - header_len;
    rc = yvex_safetensors_parse_header(json, payload_bytes, shard_path, table, err);
    if (rc != YVEX_OK) {
        table->summary.malformed_shard_count++;
    }
    free(json);
    return rc;
}
