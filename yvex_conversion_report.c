/*
 * YVEX - Conversion reports and single-tensor GGUF writer
 */
#include "yvex_conversion_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/gguf.h>
#include <yvex/weights.h>

#define CV_ALIGN 32ull

static int exists_path(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int w32(FILE *fp, unsigned int v, yvex_error *err)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xffu);
    b[1] = (unsigned char)((v >> 8) & 0xffu);
    b[2] = (unsigned char)((v >> 16) & 0xffu);
    b[3] = (unsigned char)((v >> 24) & 0xffu);
    if (fwrite(b, 1, 4, fp) != 4) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int w64(FILE *fp, unsigned long long v, yvex_error *err)
{
    unsigned char b[8];
    unsigned int i;
    for (i = 0; i < 8u; ++i) b[i] = (unsigned char)((v >> (i * 8u)) & 0xffull);
    if (fwrite(b, 1, 8, fp) != 8) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int wstr(FILE *fp, const char *s, yvex_error *err)
{
    unsigned long long len;
    if (!s) s = "";
    len = (unsigned long long)strlen(s);
    if (w64(fp, len, err) != YVEX_OK) return YVEX_ERR_IO;
    if (len && fwrite(s, 1, (size_t)len, fp) != (size_t)len) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "string write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int meta_string(FILE *fp, const char *key, const char *value, yvex_error *err)
{
    if (wstr(fp, key, err) != YVEX_OK) return YVEX_ERR_IO;
    if (w32(fp, 8u, err) != YVEX_OK) return YVEX_ERR_IO;
    return wstr(fp, value, err);
}

static int meta_u32(FILE *fp, const char *key, unsigned int value, yvex_error *err)
{
    if (wstr(fp, key, err) != YVEX_OK) return YVEX_ERR_IO;
    if (w32(fp, 4u, err) != YVEX_OK) return YVEX_ERR_IO;
    return w32(fp, value, err);
}

static int pad(FILE *fp, yvex_error *err)
{
    long pos = ftell(fp);
    unsigned long long rem;
    unsigned long long n;
    unsigned char z[32];
    memset(z, 0, sizeof(z));
    if (pos < 0) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "ftell failed");
        return YVEX_ERR_IO;
    }
    rem = ((unsigned long long)pos) % CV_ALIGN;
    n = rem == 0 ? 0 : CV_ALIGN - rem;
    if (n && fwrite(z, 1, (size_t)n, fp) != (size_t)n) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "padding write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_conversion_validate_roundtrip(const char *path, yvex_error *err)
{
    yvex_artifact_options ao;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_materialize_options mo;
    int rc;

    memset(&ao, 0, sizeof(ao));
    memset(&mo, 0, sizeof(mo));
    ao.path = path;
    ao.readonly = 1;
    mo.backend_name = "cpu";
    rc = yvex_artifact_open(&artifact, &ao, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc == YVEX_OK) rc = yvex_backend_open_cpu(&backend, err);
    if (rc == YVEX_OK) rc = yvex_weight_table_materialize(&weights, artifact, gguf, tensors, backend, &mo, err);
    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

int yvex_conversion_write_single_gguf(const yvex_conversion_options *options,
                                      const yvex_conversion_tensor_plan *plan,
                                      const unsigned char *payload,
                                      unsigned long long payload_len,
                                      yvex_conversion_summary *summary,
                                      yvex_error *err)
{
    FILE *fp;
    const yvex_native_weight_info *native;
    unsigned int i;
    long bytes;
    const char *arch_meta;

    if (!options || !plan || !payload || !summary || !options->out_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_gguf", "invalid emit arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->overwrite && exists_path(options->out_path)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_gguf", "refusing to overwrite output");
        return YVEX_ERR_INVALID_ARG;
    }
    native = plan->native;
    fp = fopen(options->out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "conversion_gguf", "cannot open output: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    arch_meta = (options->architecture && strncmp(options->architecture, "qwen", 4) == 0) ? "qwen" : "deepseek";
    if (w32(fp, YVEX_GGUF_MAGIC, err) != YVEX_OK ||
        w32(fp, 3u, err) != YVEX_OK ||
        w64(fp, 1ull, err) != YVEX_OK ||
        w64(fp, 5ull, err) != YVEX_OK ||
        meta_string(fp, "general.architecture", arch_meta, err) != YVEX_OK ||
        meta_string(fp, "general.name", "yvex-converted-selected-tensor", err) != YVEX_OK ||
        meta_u32(fp, "general.alignment", 32u, err) != YVEX_OK ||
        meta_u32(fp, "general.file_type", plan->ggml_type, err) != YVEX_OK ||
        meta_u32(fp, "qwen.context_length", 32768u, err) != YVEX_OK ||
        wstr(fp, plan->target_name, err) != YVEX_OK ||
        w32(fp, native->rank, err) != YVEX_OK) {
        fclose(fp);
        return YVEX_ERR_IO;
    }
    for (i = 0; i < native->rank; ++i) {
        unsigned int src_i = native->rank == 2 ? 1u - i : i;
        if (w64(fp, native->dims[src_i], err) != YVEX_OK) {
            fclose(fp);
            return YVEX_ERR_IO;
        }
    }
    if (w32(fp, plan->ggml_type, err) != YVEX_OK ||
        w64(fp, 0ull, err) != YVEX_OK ||
        pad(fp, err) != YVEX_OK) {
        fclose(fp);
        return YVEX_ERR_IO;
    }
    if (payload_len && fwrite(payload, 1, (size_t)payload_len, fp) != (size_t)payload_len) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "payload write failed");
        return YVEX_ERR_IO;
    }
    fflush(fp);
    bytes = ftell(fp);
    fclose(fp);
    summary->bytes_written = bytes > 0 ? (unsigned long long)bytes : payload_len;
    if (yvex_conversion_validate_roundtrip(options->out_path, err) != YVEX_OK) {
        return yvex_error_code(err);
    }
    summary->roundtrip_validated = 1;
    return YVEX_OK;
}

static void json_escape(FILE *fp, const char *s)
{
    fputc('"', fp);
    for (; s && *s; ++s) {
        if (*s == '"' || *s == '\\') fputc('\\', fp);
        fputc(*s, fp);
    }
    fputc('"', fp);
}

int yvex_conversion_report_plan_json(FILE *fp,
                                     const yvex_conversion_options *options,
                                     const yvex_conversion_summary *summary,
                                     const yvex_conversion_tensor_plan *plans,
                                     unsigned long long plan_count,
                                     yvex_error *err)
{
    unsigned long long i;
    if (!fp || !options || !summary) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_plan_json", "invalid JSON arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(fp, "{\n  \"schema\": \"yvex.conversion_plan.v1\",\n");
    fprintf(fp, "  \"architecture\": ");
    json_escape(fp, options->architecture);
    fprintf(fp, ",\n  \"native_tensors\": %llu,\n", summary->native_tensor_count);
    fprintf(fp, "  \"planned_tensors\": %llu,\n", summary->planned_tensor_count);
    fprintf(fp, "  \"unmapped_tensors\": %llu,\n", summary->unmapped_tensor_count);
    fprintf(fp, "  \"unsupported_qtypes\": %llu,\n", summary->unsupported_qtype_count);
    fprintf(fp, "  \"tensors\": [\n");
    for (i = 0; i < plan_count; ++i) {
        fprintf(fp, "    {\"native\": ");
        json_escape(fp, plans[i].native_name);
        fprintf(fp, ", \"target\": ");
        json_escape(fp, plans[i].target_name);
        fprintf(fp, ", \"role\": ");
        json_escape(fp, yvex_tensor_role_name(plans[i].role));
        fprintf(fp, ", \"qtype\": ");
        json_escape(fp, plans[i].target_qtype ? plans[i].target_qtype : "");
        fprintf(fp, ", \"status\": ");
        json_escape(fp, yvex_convert_tensor_status_name(plans[i].status));
        fprintf(fp, "}%s\n", i + 1 == plan_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    return ferror(fp) ? YVEX_ERR_IO : YVEX_OK;
}
