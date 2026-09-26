/* Persist the compiler's physical tensor program and exact parameter-role map. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/tensor_binding.h>
#include <yvex/internal/core.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TENSOR_BINDING_MAX_BYTES (64u * 1024u * 1024u)
static const unsigned char binding_magic[8] = {'Y','V','E','X','T','B','0','1'};

struct yvex_tensor_binding {
    yvex_tensor_binding_summary summary;
    yvex_program_physical *program;
    char (*names)[256];
};

static int binding_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.tensor-binding", reason);
    return status;
}

static int binding_u64(yvex_core_bytes *bytes, unsigned long long value)
{
    unsigned char encoded[8];
    for (unsigned int i = 0u; i < 8u; ++i) encoded[i] = (unsigned char)(value >> (i * 8u));
    return yvex_core_bytes_append(bytes, encoded, sizeof(encoded));
}

static int binding_take_u64(const unsigned char *bytes, size_t size, size_t *offset,
    unsigned long long *value)
{
    if (*offset > size || size - *offset < 8u) return 0;
    *value = 0u;
    for (unsigned int i = 0u; i < 8u; ++i)
        *value |= (unsigned long long)bytes[*offset + i] << (i * 8u);
    *offset += 8u;
    return 1;
}

static int binding_digest(const unsigned char *bytes, size_t count,
    unsigned char raw[YVEX_SHA256_DIGEST_BYTES], char hex[YVEX_SHA256_HEX_BYTES])
{
    yvex_sha256 hash;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, bytes, count) || !yvex_sha256_final(&hash, raw)) return 0;
    yvex_sha256_hex(raw, hex);
    return 1;
}

static int binding_write_all(int fd, const unsigned char *bytes, size_t count)
{
    while (count) {
        ssize_t written = write(fd, bytes, count);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return 0;
        bytes += (size_t)written;
        count -= (size_t)written;
    }
    return 1;
}

static int binding_publish_bytes(const char *path, const unsigned char *bytes,
    size_t count, yvex_error *err)
{
    size_t length = strlen(path);
    if (length > SIZE_MAX - 16u) return binding_refuse(err, YVEX_ERR_BOUNDS, "binding path is too long");
    char *temporary = malloc(length + 16u);
    if (!temporary) return binding_refuse(err, YVEX_ERR_NOMEM, "binding temporary path allocation failed");
    snprintf(temporary, length + 16u, "%s.part.XXXXXX", path);
    int fd = mkstemp(temporary);
    if (fd < 0) {
        free(temporary);
        return binding_refuse(err, YVEX_ERR_IO, "binding temporary file cannot be created");
    }
    int good = fchmod(fd, 0600) == 0 && binding_write_all(fd, bytes, count) && fsync(fd) == 0;
    if (close(fd) != 0) good = 0;
    if (good) good = link(temporary, path) == 0;
    unlink(temporary);
    free(temporary);
    return good ? YVEX_OK : binding_refuse(err, YVEX_ERR_IO,
        "immutable binding publication failed or identity path already exists");
}

int yvex_tensor_binding_publish(const char *path, const yvex_tensor_binding_build_request *request,
    yvex_tensor_binding_summary *out, yvex_error *err)
{
    const yvex_program_physical_summary *physical =
        request ? yvex_program_physical_summary_get(request->program) : NULL;
    if (out) memset(out, 0, sizeof(*out));
    if (!path || !*path || !request || !out || !physical ||
        request->schema_version != YVEX_TENSOR_BINDING_SCHEMA_V1 ||
        !yvex_sha256_hex_valid(request->source_identity) ||
        !yvex_sha256_hex_valid(request->tokenizer_identity) ||
        !yvex_sha256_hex_valid(request->logical_model_identity) ||
        !request->source_bytes || !request->source_tensor_count ||
        request->input_format != YVEX_TENSOR_INPUT_TOKEN_TYPE_DUAL_ROPE_V1 ||
        !request->rotary_width || request->rotary_width > 256u ||
        request->rotary_width % 2u || !request->primary_theta || !request->secondary_theta ||
        !request->token_domain_size || !request->type_domain_size ||
        request->marker_token_id >= request->token_domain_size ||
        physical->input_count != 6u || physical->result_count != 1u ||
        !request->parameter_name || !request->parameter_count || request->parameter_count > 4096u)
        return binding_refuse(err, YVEX_ERR_INVALID_ARG, "sealed model, tokenizer, program and roles required");
    yvex_core_bytes wire = {.maximum = TENSOR_BINDING_MAX_BYTES};
    yvex_core_bytes package = {.maximum = TENSOR_BINDING_MAX_BYTES};
    int rc = yvex_program_physical_encode(request->program, &wire, err);
    if (rc != YVEX_OK) return rc;
    int good = yvex_core_bytes_append(&package, binding_magic, sizeof(binding_magic)) &&
        binding_u64(&package, YVEX_TENSOR_BINDING_SCHEMA_V1) &&
        yvex_core_bytes_append(&package, request->source_identity, 64u) &&
        yvex_core_bytes_append(&package, request->tokenizer_identity, 64u) &&
        yvex_core_bytes_append(&package, request->logical_model_identity, 64u) &&
        yvex_core_bytes_append(&package, physical->identity, 64u) &&
        binding_u64(&package, request->source_bytes) &&
        binding_u64(&package, request->source_tensor_count) &&
        binding_u64(&package, request->input_format) &&
        binding_u64(&package, request->rotary_width) &&
        binding_u64(&package, request->primary_theta) &&
        binding_u64(&package, request->secondary_theta) &&
        binding_u64(&package, request->token_domain_size) &&
        binding_u64(&package, request->type_domain_size) &&
        binding_u64(&package, request->marker_token_id) &&
        binding_u64(&package, request->parameter_count) && binding_u64(&package, wire.count);
    for (size_t i = 0u; good && i < request->parameter_count; ++i) {
        char name[256] = {0};
        rc = request->parameter_name(request->parameter_context, i, name, err);
        const char *end = rc == YVEX_OK ? memchr(name, '\0', sizeof(name)) : NULL;
        size_t length = end ? (size_t)(end - name) : 0u;
        if (rc != YVEX_OK || !length || length > 255u) { good = 0; break; }
        good = binding_u64(&package, length) && yvex_core_bytes_append(&package, name, length);
    }
    if (good) good = yvex_core_bytes_append(&package, wire.data, wire.count);
    unsigned char raw[YVEX_SHA256_DIGEST_BYTES];
    char identity[YVEX_SHA256_HEX_BYTES];
    if (good) good = binding_digest(package.data, package.count, raw, identity) &&
        yvex_core_bytes_append(&package, raw, sizeof(raw));
    if (good) rc = binding_publish_bytes(path, package.data, package.count, err);
    else rc = binding_refuse(err, YVEX_ERR_FORMAT, "bounded parameter directory or binding encoding failed");
    if (rc == YVEX_OK) {
        out->schema_version = YVEX_TENSOR_BINDING_SCHEMA_V1;
        yvex_core_text_copy(out->identity, sizeof(out->identity), identity);
        yvex_core_text_copy(out->source_identity, sizeof(out->source_identity), request->source_identity);
        yvex_core_text_copy(out->tokenizer_identity, sizeof(out->tokenizer_identity), request->tokenizer_identity);
        yvex_core_text_copy(out->logical_model_identity, sizeof(out->logical_model_identity),
            request->logical_model_identity);
        yvex_core_text_copy(out->physical_program_identity, sizeof(out->physical_program_identity),
            physical->identity);
        out->parameter_count = request->parameter_count;
        out->source_bytes = request->source_bytes;
        out->source_tensor_count = request->source_tensor_count;
        out->input_format = request->input_format;
        out->rotary_width = request->rotary_width;
        out->primary_theta = request->primary_theta;
        out->secondary_theta = request->secondary_theta;
        out->token_domain_size = request->token_domain_size;
        out->type_domain_size = request->type_domain_size;
        out->marker_token_id = request->marker_token_id;
        out->file_bytes = package.count;
    }
    free(package.data);
    free(wire.data);
    return rc;
}

static int binding_read_all(const char *path, unsigned char **out,
    size_t *out_size, yvex_error *err)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size < 8 || st.st_size > TENSOR_BINDING_MAX_BYTES) {
        if (fd >= 0) close(fd);
        return binding_refuse(err, YVEX_ERR_IO, "bounded regular binding file required");
    }
    unsigned char *bytes = malloc((size_t)st.st_size);
    if (!bytes) { close(fd); return binding_refuse(err, YVEX_ERR_NOMEM, "binding input allocation failed"); }
    size_t offset = 0u;
    while (offset < (size_t)st.st_size) {
        ssize_t n = read(fd, bytes + offset, (size_t)st.st_size - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        offset += (size_t)n;
    }
    int good = offset == (size_t)st.st_size;
    if (close(fd) != 0) good = 0;
    if (!good) {
        free(bytes);
        return binding_refuse(err, YVEX_ERR_IO, "binding file is truncated or changed during read");
    }
    *out = bytes;
    *out_size = offset;
    return YVEX_OK;
}

int yvex_tensor_binding_open(yvex_tensor_binding **out, const char *path, yvex_error *err)
{
    if (out) *out = NULL;
    if (!out || !path || !*path)
        return binding_refuse(err, YVEX_ERR_INVALID_ARG, "binding path and owner required");
    unsigned char *bytes = NULL;
    size_t size = 0u, offset = 0u;
    int rc = binding_read_all(path, &bytes, &size, err);
    if (rc != YVEX_OK) return rc;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    char identity[YVEX_SHA256_HEX_BYTES];
    if (size < 8u + 8u + 4u * 64u + 88u + sizeof(digest) ||
        !binding_digest(bytes, size - sizeof(digest), digest, identity) ||
        memcmp(bytes + size - sizeof(digest), digest, sizeof(digest)) ||
        memcmp(bytes, binding_magic, sizeof(binding_magic))) {
        free(bytes);
        return binding_refuse(err, YVEX_ERR_FORMAT, "binding header or digest is invalid");
    }
    offset = sizeof(binding_magic);
    unsigned long long schema, count, program_bytes;
    if (!binding_take_u64(bytes, size, &offset, &schema) || schema != YVEX_TENSOR_BINDING_SCHEMA_V1) {
        free(bytes);
        return binding_refuse(err, YVEX_ERR_FORMAT, "binding schema is unsupported");
    }
    yvex_tensor_binding *binding = calloc(1u, sizeof(*binding));
    if (!binding) { free(bytes); return binding_refuse(err, YVEX_ERR_NOMEM, "binding owner allocation failed"); }
    binding->summary.schema_version = YVEX_TENSOR_BINDING_SCHEMA_V1;
    yvex_core_text_copy(binding->summary.identity, sizeof(binding->summary.identity), identity);
    char *fields[] = {binding->summary.source_identity, binding->summary.tokenizer_identity,
        binding->summary.logical_model_identity, binding->summary.physical_program_identity};
    for (size_t i = 0u; i < 4u; ++i) {
        memcpy(fields[i], bytes + offset, 64u);
        fields[i][64] = '\0';
        offset += 64u;
        if (!yvex_sha256_hex_valid(fields[i])) { rc = YVEX_ERR_FORMAT; break; }
    }
    if (rc == YVEX_OK && (!binding_take_u64(bytes, size, &offset,
            &binding->summary.source_bytes) || !binding->summary.source_bytes ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.source_tensor_count) || !binding->summary.source_tensor_count ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.input_format) ||
        binding->summary.input_format != YVEX_TENSOR_INPUT_TOKEN_TYPE_DUAL_ROPE_V1 ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.rotary_width) || !binding->summary.rotary_width ||
        binding->summary.rotary_width > 256u || binding->summary.rotary_width % 2u ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.primary_theta) || !binding->summary.primary_theta ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.secondary_theta) || !binding->summary.secondary_theta ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.token_domain_size) || !binding->summary.token_domain_size ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.type_domain_size) || !binding->summary.type_domain_size ||
        !binding_take_u64(bytes, size, &offset,
            &binding->summary.marker_token_id) ||
        binding->summary.marker_token_id >= binding->summary.token_domain_size ||
        !binding_take_u64(bytes, size, &offset, &count) ||
        !binding_take_u64(bytes, size, &offset, &program_bytes) ||
        !count || count > 4096u || program_bytes > TENSOR_BINDING_MAX_BYTES)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) {
        binding->names = calloc((size_t)count, sizeof(*binding->names));
        if (!binding->names) rc = YVEX_ERR_NOMEM;
    }
    for (size_t i = 0u; rc == YVEX_OK && i < count; ++i) {
        unsigned long long length;
        if (!binding_take_u64(bytes, size, &offset, &length) || !length || length > 255u ||
            offset > size || length > size - offset) { rc = YVEX_ERR_FORMAT; break; }
        memcpy(binding->names[i], bytes + offset, (size_t)length);
        offset += (size_t)length;
    }
    if (rc == YVEX_OK && (offset > size - sizeof(digest) ||
        program_bytes != size - sizeof(digest) - offset)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK)
        rc = yvex_program_physical_decode(&binding->program, bytes + offset, (size_t)program_bytes, err);
    if (rc == YVEX_OK && (strcmp(yvex_program_physical_summary_get(binding->program)->identity,
            binding->summary.physical_program_identity) ||
        yvex_program_physical_summary_get(binding->program)->input_count != 6u ||
        yvex_program_physical_summary_get(binding->program)->result_count != 1u)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) {
        binding->summary.parameter_count = count;
        binding->summary.file_bytes = size;
        *out = binding;
        free(bytes);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    free(bytes);
    yvex_tensor_binding_close(&binding);
    if (!yvex_error_is_set(err)) binding_refuse(err, rc, "binding contents are malformed or inconsistent");
    return rc;
}

const yvex_tensor_binding_summary *yvex_tensor_binding_summary_get(const yvex_tensor_binding *binding)
{
    return binding ? &binding->summary : NULL;
}

const yvex_program_physical *yvex_tensor_binding_program(const yvex_tensor_binding *binding)
{
    return binding ? binding->program : NULL;
}

int yvex_tensor_binding_parameter_name(void *opaque, unsigned long long ordinal,
    char name[256], yvex_error *err)
{
    const yvex_tensor_binding *binding = opaque;
    if (!binding || !name || ordinal >= binding->summary.parameter_count)
        return binding_refuse(err, YVEX_ERR_BOUNDS, "binding parameter ordinal is unavailable");
    yvex_core_text_copy(name, 256u, binding->names[ordinal]);
    return YVEX_OK;
}

void yvex_tensor_binding_close(yvex_tensor_binding **binding)
{
    if (!binding || !*binding) return;
    yvex_program_physical_close(&(*binding)->program);
    free((*binding)->names);
    free(*binding);
    *binding = NULL;
}
