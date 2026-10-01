/*
 * Provide approved direct text output calls for operator normal/table/audit text.
 *
 * General stdio wrappers live here; typed host/turn renderers live in events.c. Wrappers preserve stdio
 * return behavior where legacy code checks it. Writer calls serialize existing facts only and do
 * not create capability.
 */
#include <build_commit.h>
#include "src/cli/io/private.h"
#include "src/cli/io/terminal/private.h"
#include <yvex/internal/cli_presentation.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <operator/registry.h>
#include <yvex/internal/core.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum { COMPLETION_CANDIDATE_CAP = 256, COMPLETION_TEXT_CAP = 128 };
typedef struct {
    char text[COMPLETION_CANDIDATE_CAP][COMPLETION_TEXT_CAP];
    size_t count;
} completion_candidates;
static int completion_visible(const yvex_operator_descriptor *descriptor)
{
    return descriptor->cli_projection &&
           descriptor->visibility != YVEX_OPERATOR_VISIBILITY_REMOVED &&
           descriptor->visibility != YVEX_OPERATOR_VISIBILITY_API_ONLY &&
           descriptor->visibility != YVEX_OPERATOR_VISIBILITY_TEST_ONLY;
}
static int completion_prefix_matches(const yvex_operator_descriptor *descriptor,
                                     size_t count, const char *const *words)
{
    size_t index;
    if (count > descriptor->command_word_count) return 0;
    for (index = 0u; index < count; ++index)
        if (strcmp(descriptor->command_words[index], words[index])) return 0;
    return 1;
}
static void completion_add(completion_candidates *candidates, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;
    size_t index;
    if (!value[0] || strlen(value) >= COMPLETION_TEXT_CAP) return;
    while (*cursor) {
        if (!(isalnum(*cursor) || strchr("._:/@+-", *cursor))) return;
        cursor++;
    }
    for (index = 0u; index < candidates->count; ++index)
        if (!strcmp(candidates->text[index], value)) return;
    if (candidates->count >= COMPLETION_CANDIDATE_CAP) return;
    (void)snprintf(candidates->text[candidates->count], COMPLETION_TEXT_CAP, "%s", value);
    candidates->count++;
}
static void completion_add_metadata(completion_candidates *candidates, const char *values)
{
    const char *cursor = values;
    if (!strcmp(values, "none")) return;
    while (*cursor) {
        const char *end = strchr(cursor, '|');
        size_t extent = end ? (size_t)(end - cursor) : strlen(cursor);
        char item[COMPLETION_TEXT_CAP];
        if (extent < sizeof(item)) {
            memcpy(item, cursor, extent);
            item[extent] = '\0';
            completion_add(candidates, item);
        }
        if (!end) break;
        cursor = end + 1;
    }
}

static completion_candidates completion_collect(size_t prefix_count,
                                                const char *const *prefix)
{
    completion_candidates candidates = {{{0}}, 0u};
    size_t descriptor_index;
    for (descriptor_index = 0u; descriptor_index < yvex_operator_descriptor_count;
         ++descriptor_index) {
        const yvex_operator_descriptor *descriptor =
            &yvex_operator_descriptors[descriptor_index];
        size_t index;
        if (completion_visible(descriptor) &&
            prefix_count == descriptor->command_word_count + 1u &&
            completion_prefix_matches(descriptor, descriptor->command_word_count, prefix)) {
            for (index = 0u; index < descriptor->flag_count; ++index)
                if (!strcmp(descriptor->flags[index].name, prefix[prefix_count - 1u]))
                    completion_add_metadata(&candidates, descriptor->flags[index].enum_values);
            continue;
        }
        if (!completion_visible(descriptor) ||
            (!prefix_count && descriptor->visibility !=
                                  YVEX_OPERATOR_VISIBILITY_PRODUCT_DEFAULT) ||
            !completion_prefix_matches(descriptor, prefix_count, prefix))
            continue;
        if (descriptor->command_word_count > prefix_count) {
            completion_add(&candidates, descriptor->command_words[prefix_count]);
            continue;
        }
        for (index = 0u; index < descriptor->flag_count; ++index) {
            completion_add(&candidates, descriptor->flags[index].name);
            completion_add_metadata(&candidates, descriptor->flags[index].aliases);
        }
        for (index = 0u; index < descriptor->argument_count; ++index)
            completion_add_metadata(&candidates, descriptor->arguments[index].enum_values);
    }
    return candidates;
}

static void completion_emit_case(FILE *output, const char *shell,
                                 size_t prefix_count, const char *const *prefix)
{
    completion_candidates candidates = completion_collect(prefix_count, prefix);
    size_t index;
    if (!candidates.count) return;
    if (!strcmp(shell, "fish")) fputs("    case '", output);
    else fputs("    '", output);
    for (index = 0u; index < prefix_count; ++index)
        fprintf(output, "%s%s", index ? " " : "", prefix[index]);
    if (!strcmp(shell, "fish")) fputs("'\n      set candidates", output);
    else fputs("') candidates='", output);
    for (index = 0u; index < candidates.count; ++index)
        fprintf(output, " %s", candidates.text[index]);
    if (!strcmp(shell, "fish")) fputc('\n', output);
    else fputs(" ' ;;\n", output);
}

static void completion_emit_cases(FILE *output, const char *shell)
{
    size_t descriptor_index, prefix_count, prior;
    for (descriptor_index = 0u; descriptor_index < yvex_operator_descriptor_count;
         ++descriptor_index) {
        const yvex_operator_descriptor *descriptor =
            &yvex_operator_descriptors[descriptor_index];
        if (!completion_visible(descriptor)) continue;
        for (prefix_count = 0u; prefix_count <= descriptor->command_word_count;
             ++prefix_count) {
            int seen = 0;
            for (prior = 0u; prior < descriptor_index && !seen; ++prior) {
                const yvex_operator_descriptor *candidate =
                    &yvex_operator_descriptors[prior];
                if (completion_visible(candidate) &&
                    candidate->command_word_count >= prefix_count &&
                    completion_prefix_matches(candidate, prefix_count,
                                              descriptor->command_words))
                    seen = 1;
            }
            if (!seen)
                completion_emit_case(output, shell, prefix_count,
                                     descriptor->command_words);
        }
        if (descriptor->command_word_count < 63u) {
            size_t flag;
            const char *words[64];
            for (prefix_count = 0u; prefix_count < descriptor->command_word_count; ++prefix_count)
                words[prefix_count] = descriptor->command_words[prefix_count];
            for (flag = 0u; flag < descriptor->flag_count; ++flag) {
                if (!strcmp(descriptor->flags[flag].enum_values, "none")) continue;
                words[prefix_count] = descriptor->flags[flag].name;
                completion_emit_case(output, shell, prefix_count + 1u, words);
            }
        }
    }
}

int yvex_cli_completion_command(int argc, char **argv, size_t consumed)
{
    const char *shell = consumed + 1u < (size_t)argc ? argv[consumed + 1u] : NULL;
    if (!shell) return 2;
    if (!strcmp(shell, "bash")) {
        fputs("_yvex_complete() {\n"
              "  local cur=${COMP_WORDS[COMP_CWORD]} path='' candidates=''\n"
              "  if (( COMP_CWORD > 1 )); then "
              "path=${COMP_WORDS[*]:1:$((COMP_CWORD-1))}; fi\n"
              "  case \"$path\" in\n", stdout);
        completion_emit_cases(stdout, shell);
        fputs("  esac\n  COMPREPLY=( $(compgen -W \"$candidates\" -- \"$cur\") )\n"
              "}\ncomplete -F _yvex_complete yvex\n", stdout);
        return 0;
    }
    if (!strcmp(shell, "zsh")) {
        fputs("#compdef yvex\n_yvex_complete() {\n"
              "  local path='' candidates=''\n"
              "  if (( CURRENT > 2 )); then path=${(j: :)words[2,$((CURRENT-1))]}; fi\n"
              "  case \"$path\" in\n", stdout);
        completion_emit_cases(stdout, shell);
        fputs("  esac\n  compadd -- ${(z)candidates}\n}\ncompdef _yvex_complete yvex\n", stdout);
        return 0;
    }
    if (!strcmp(shell, "fish")) {
        fputs("function __yvex_candidates\n"
              "  set -l tokens (commandline -opc)\n"
              "  set -e tokens[1]\n"
              "  set -l path (string join ' ' $tokens)\n"
              "  set -l candidates\n"
              "  switch $path\n", stdout);
        completion_emit_cases(stdout, shell);
        fputs("  end\n  printf '%s\\n' $candidates\nend\n"
              "complete -c yvex -f -a '(__yvex_candidates)'\n", stdout);
        return 0;
    }
    fprintf(stderr, "yvex: completion shell must be bash, zsh, or fish\n");
    return 2;
}

int yvex_cli_out_vwritef(FILE *fp, const char *fmt, va_list ap)
{
    return vfprintf(fp ? fp : stdout, fmt ? fmt : "", ap);
}

int yvex_cli_out_writef(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = yvex_cli_out_vwritef(fp, fmt, ap);
    va_end(ap);
    return rc;
}

int yvex_cli_out_puts(FILE *fp, const char *text)
{
    return fputs(text ? text : "", fp ? fp : stdout);
}

int yvex_cli_out_fputs(const char *text, FILE *fp)
{
    return yvex_cli_out_puts(fp, text);
}

int yvex_cli_out_char(FILE *fp, int ch)
{
    return fputc(ch, fp ? fp : stdout);
}

int yvex_cli_out_flush(FILE *fp)
{
    FILE *stream = fp ? fp : stdout;

    return fflush(stream) == 0 && !ferror(stream) ? YVEX_OK : YVEX_ERR_IO;
}

FILE *yvex_cli_out_stdout(void)
{
    return stdout;
}

FILE *yvex_cli_out_stderr(void)
{
    return stderr;
}

void yvex_cli_terminal_style_get(FILE *fp, yvex_cli_terminal_style *style)
{
    FILE *stream = fp ? fp : stdout;
    const char *terminal;

    if (!style) return;
    memset(style, 0, sizeof(*style));
    style->reset = "";
    style->strong = "";
    style->accent = "";
    style->dim = "";
    style->success = "";
    style->warning = "";
    style->error = "";
    terminal = getenv("TERM");
    if (!yvex_cli_terminal_interactive(stream) || getenv("NO_COLOR") ||
        (terminal && !strcmp(terminal, "dumb")))
        return;
    style->reset = yvex_cli_present_style(YVEX_CLI_TEXT_NORMAL);
    style->strong = yvex_cli_present_style(YVEX_CLI_TEXT_STRONG);
    style->accent = yvex_cli_present_style(YVEX_CLI_TEXT_ACCENT);
    style->dim = yvex_cli_present_style(YVEX_CLI_TEXT_DIM);
    style->success = yvex_cli_present_style(YVEX_CLI_TEXT_SUCCESS);
    style->warning = yvex_cli_present_style(YVEX_CLI_TEXT_WARNING);
    style->error = yvex_cli_present_style(YVEX_CLI_TEXT_ERROR);
}



void yvex_cli_out_repl_catalog(void)
{
    size_t index, prior;
    for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
        const yvex_operator_descriptor *row = &yvex_operator_descriptors[index];
        yvex_cli_present_field fields[64];
        size_t next, count = 0u;
        if (!strcmp(row->slash_projection, "none")) continue;
        for (prior = 0u; prior < index; ++prior)
            if (strcmp(yvex_operator_descriptors[prior].slash_projection, "none") &&
                !strcmp(yvex_operator_descriptors[prior].slash_group, row->slash_group)) break;
        if (prior != index) continue;
        for (next = index; next < yvex_operator_descriptor_count && count < 64u; ++next) {
            const yvex_operator_descriptor *candidate = &yvex_operator_descriptors[next];
            if (strcmp(candidate->slash_projection, "none") &&
                !strcmp(candidate->slash_group, row->slash_group))
                fields[count++] = (yvex_cli_present_field){candidate->slash_projection,
                    candidate->summary, YVEX_CLI_TEXT_NORMAL};
        }
        (void)yvex_cli_present_record(stdout, row->slash_group, fields, count);
    }
    {
        const yvex_cli_present_field keys[] = {
            {"Ctrl-C", "Cancel work or clear input; repeat to leave", YVEX_CLI_TEXT_NORMAL},
            {"Ctrl-D", "Leave on empty input; otherwise delete", YVEX_CLI_TEXT_NORMAL},
            {"Tab", "Open candidates; Enter selects, then submits", YVEX_CLI_TEXT_NORMAL}};
        (void)yvex_cli_present_record(stdout, "Keyboard", keys, 3u);
    }
}
void yvex_cli_out_line(FILE *fp, const char *text)
{
    (void)yvex_cli_out_puts(fp, text);
    (void)yvex_cli_out_char(fp, '\n');
}
void yvex_cli_out_lines(FILE *fp,
                        const char *const *lines,
                        size_t line_count)
{
    size_t i;
    if (!lines) {
        return;
    }
    for (i = 0; i < line_count; ++i) {
        yvex_cli_out_line(fp, lines[i]);
    }
}
void yvex_cli_out_kv_str(FILE *fp, const char *key, const char *value)
{
    const yvex_cli_present_field field = {key, value, YVEX_CLI_TEXT_NORMAL};
    (void)yvex_cli_present_fields(fp, &field, 1u, 2u);
}
void yvex_cli_out_kv_bool(FILE *fp, const char *key, int value)
{
    yvex_cli_out_kv_str(fp, key, value ? "true" : "false");
}
int yvex_cli_out_fields(FILE *fp, const void *object,
                        const yvex_cli_field_spec *fields, size_t field_count)
{
    const unsigned char *base = object;
    yvex_cli_present_field *intent;
    char (*numbers)[128];
    size_t i;
    int rc = -1;
    if (!object || (!fields && field_count) || field_count > 4096u) return -1;
    intent = calloc(field_count ? field_count : 1u, sizeof(*intent));
    numbers = calloc(field_count ? field_count : 1u, sizeof(*numbers));
    if (!intent || !numbers) goto done;
    for (i = 0u; i < field_count; ++i) {
        const yvex_cli_field_spec *field = &fields[i];
        const void *value = base + field->offset;
        const char *text = numbers[i];
        switch (field->kind) {
        case YVEX_CLI_FIELD_TEXT: text = *(const char *const *)value; break;
        case YVEX_CLI_FIELD_TEXT_ARRAY: text = value; break;
        case YVEX_CLI_FIELD_U64: snprintf(numbers[i], 128u, "%llu", *(const unsigned long long *)value); break;
        case YVEX_CLI_FIELD_U32: snprintf(numbers[i], 128u, "%u", *(const unsigned int *)value); break;
        case YVEX_CLI_FIELD_I32: snprintf(numbers[i], 128u, "%d", *(const int *)value); break;
        case YVEX_CLI_FIELD_BOOL: text = *(const int *)value ? "true" : "false"; break;
        case YVEX_CLI_FIELD_DOUBLE: snprintf(numbers[i], 128u, "%.17g", *(const double *)value); break;
        case YVEX_CLI_FIELD_FLOAT9: snprintf(numbers[i], 128u, "%.9g", *(const double *)value); break;
        case YVEX_CLI_FIELD_HEX64: snprintf(numbers[i], 128u, "%016llx", *(const unsigned long long *)value); break;
        default: goto done;
        }
        intent[i] = (yvex_cli_present_field){field->key,
            text && text[0] ? text : field->fallback ? field->fallback : "unknown", YVEX_CLI_TEXT_NORMAL};
    }
    rc = yvex_cli_present_fields(fp, intent, field_count, 2u) == YVEX_OK ? 0 : -1;
done:
    free(numbers); free(intent);
    return rc;
}
int print_yvex_error(const yvex_error *err, int exit_code)
{
    char code[32];
    yvex_cli_present_field facts[3];
    snprintf(code, sizeof(code), "%d", (int)yvex_error_code(err));
    facts[0] = (yvex_cli_present_field){"owner", yvex_error_where(err), YVEX_CLI_TEXT_DIM};
    facts[1] = (yvex_cli_present_field){"reason", yvex_error_message(err), YVEX_CLI_TEXT_ERROR};
    facts[2] = (yvex_cli_present_field){"status", code, YVEX_CLI_TEXT_DIM};
    (void)yvex_cli_present_record(stderr, "YVEX refusal", facts, 3u);
    return exit_code;
}
int exit_for_status(int status)
{
    switch (status) {
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}
/*
 * Parse one complete unsigned integer without accepting signs or suffixes.
 *
 * Returns false and leaves result ownership with the caller.
 */
int parse_ull_allow_zero(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;
    if (!text || !out || text[0] == '\0' || text[0] == '-') return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return 0;
    *out = value;
    return 1;
}
int parse_positive_ull(const char *text, unsigned long long *out)
{
    return parse_ull_allow_zero(text, out) && *out != 0;
}
int parse_uint_allow_zero(const char *text, unsigned int *out)
{
    unsigned long long value;
    if (!out || !parse_ull_allow_zero(text, &value) || value > UINT32_MAX) return 0;
    *out = (unsigned int)value;
    return 1;
}
void print_quoted_bytes(const char *data, unsigned long long len)
{
    unsigned long long i;
    yvex_cli_out_writef(stdout, "\"");
    for (i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];
        if (ch == '"' || ch == '\\') {
            yvex_cli_out_writef(stdout, "\\%c", (int)ch);
        } else if (ch == '\n') {
            yvex_cli_out_writef(stdout, "\\n");
        } else if (ch == '\r') {
            yvex_cli_out_writef(stdout, "\\r");
        } else if (ch == '\t') {
            yvex_cli_out_writef(stdout, "\\t");
        } else if (ch < 32 || ch > 126) {
            yvex_cli_out_writef(stdout, "\\x%02x", (unsigned int)ch);
        } else {
            yvex_cli_out_writef(stdout, "%c", (int)ch);
        }
    }
    yvex_cli_out_writef(stdout, "\"");
}
int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;
    yvex_model_ref ref;
    int rc;
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    rc = yvex_model_ref_resolve(&ref, path, NULL, err);
    if (rc != YVEX_OK) return rc;
    options.path = ref.path;
    options.readonly = 1;
    rc = yvex_artifact_open(artifact, &options, err);
    yvex_model_ref_clear(&ref);
    return rc;
}
void print_tensor_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;
    yvex_cli_out_writef(stdout, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) yvex_cli_out_writef(stdout, ",");
        yvex_cli_out_writef(stdout, "%llu", dims[i]);
    }
    yvex_cli_out_writef(stdout, "]");
}
void print_native_dims(const unsigned long long *dims, unsigned int rank)
{
    print_tensor_dims(dims, rank);
}
void print_token_ids(const yvex_tokens *tokens)
{
    unsigned long long i;
    yvex_cli_out_writef(stdout, "ids:");
    for (i = 0; i < tokens->len; ++i) yvex_cli_out_writef(stdout, " %u", tokens->ids[i]);
    yvex_cli_out_writef(stdout, "\n");
}
int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len)
{
    unsigned int *ids = NULL;
    unsigned long long len = 0;
    unsigned long long capacity = 0;
    const char *cursor = text;
    if (!text || !out_ids || !out_len) return 0;
    *out_ids = NULL;
    *out_len = 0;
    while (*cursor) {
        char *end = NULL;
        unsigned long value = strtoul(cursor, &end, 10);
        unsigned int *next;
        if (end == cursor || value > UINT32_MAX) goto fail;
        if (len == capacity) {
            unsigned long long next_capacity = capacity == 0 ? 8 : capacity * 2u;
            if (next_capacity > (unsigned long long)(SIZE_MAX / sizeof(*ids))) goto fail;
            next = realloc(ids, (size_t)next_capacity * sizeof(*ids));
            if (!next) goto fail;
            ids = next;
            capacity = next_capacity;
        }
        ids[len++] = (unsigned int)value;
        if (*end == ',') {
            cursor = end + 1;
        } else if (*end == '\0') {
            cursor = end;
        } else {
            goto fail;
        }
    }
    if (len == 0) goto fail;
    *out_ids = ids;
    *out_len = len;
    return 1;

fail:
    free(ids);
    return 0;
}

/*
 * Parse an exact-rank comma-separated tensor shape.
 *
 * Rejects invalid rank, malformed fields, zero dimensions, and overflow.
 */
int parse_dims_csv(const char *text, unsigned int rank, unsigned long long dims[4])
{
    const char *cursor = text;
    char *end = NULL;
    unsigned int i;

    if (!text || !dims || rank == 0 || rank > 4u) return 0;
    memset(dims, 0, 4u * sizeof(*dims));
    for (i = 0; i < rank; ++i) {
        errno = 0;
        dims[i] = strtoull(cursor, &end, 10);
        if (errno != 0 || end == cursor || dims[i] == 0) return 0;
        if (i + 1u < rank) {
            if (*end != ',') return 0;
            cursor = end + 1;
        } else if (*end != '\0') {
            return 0;
        }
    }
    return 1;
}

/*
 * Provide approved direct JSON text output for CLI plumbing surfaces.
 *
 * Helpers serialize only caller-provided fields and do not claim uniform JSON. JSON writer
 * primitives are not command-level JSON support by themselves.
 */
void yvex_cli_out_json_string(FILE *fp, const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    (void)yvex_cli_out_char(fp, '"');
    while (*p) {
        if (*p == '"' || *p == '\\') {
            (void)yvex_cli_out_char(fp, '\\');
            (void)yvex_cli_out_char(fp, *p);
        } else if (*p == '\n') {
            (void)yvex_cli_out_puts(fp, "\\n");
        } else if (*p == '\r') {
            (void)yvex_cli_out_puts(fp, "\\r");
        } else if (*p == '\t') {
            (void)yvex_cli_out_puts(fp, "\\t");
        } else if (*p < 0x20u) {
            (void)yvex_cli_out_writef(fp, "\\u%04x", (unsigned int)*p);
        } else {
            (void)yvex_cli_out_char(fp, *p);
        }
        ++p;
    }
    (void)yvex_cli_out_char(fp, '"');
}

static void discovery_json_list(FILE *output, const char *value, int delimiter)
{
    const char *cursor = value;
    int first = 1;
    fputc('[', output);
    if (strcmp(value, "none")) {
        while (*cursor) {
            const char *end = strchr(cursor, delimiter);
            size_t extent = end ? (size_t)(end - cursor) : strlen(cursor);
            char item[512];
            if (extent >= sizeof(item)) extent = sizeof(item) - 1u;
            memcpy(item, cursor, extent);
            item[extent] = '\0';
            if (!first) fputc(',', output);
            yvex_cli_out_json_string(output, item);
            first = 0;
            if (!end) break;
            cursor = end + 1;
        }
    }
    fputc(']', output);
}
static const char *visibility_name(yvex_operator_visibility visibility)
{
    switch (visibility) {
    case YVEX_OPERATOR_VISIBILITY_PRODUCT_DEFAULT: return "product-default";
    case YVEX_OPERATOR_VISIBILITY_PRODUCT_ADVANCED: return "product-advanced";
    case YVEX_OPERATOR_VISIBILITY_ENGINEERING: return "engineering";
    case YVEX_OPERATOR_VISIBILITY_AUTOMATION: return "automation";
    case YVEX_OPERATOR_VISIBILITY_API_ONLY: return "API-only";
    case YVEX_OPERATOR_VISIBILITY_TEST_ONLY: return "test-only";
    case YVEX_OPERATOR_VISIBILITY_REMOVED: return "removed";
    }
    return "unknown";
}
static const char *plane_name(yvex_operator_plane plane)
{
    static const char *const names[] = {
        "Compile", "Execute", "Inspect", "Integrate", "Profile", "Run", "System"};
    return (unsigned int)plane < sizeof(names) / sizeof(names[0]) ? names[plane]
                                                                  : "Unknown";
}
static const char *lane_name(yvex_operator_lane lane)
{
    switch (lane) {
    case YVEX_OPERATOR_LANE_RUNTIME_CLIENT: return "runtime-client";
    case YVEX_OPERATOR_LANE_OFFLINE_ENGINE: return "offline-engine";
    case YVEX_OPERATOR_LANE_DAEMON_ENTRYPOINT: return "host-entrypoint";
    case YVEX_OPERATOR_LANE_REPL_LOCAL: return "REPL-local";
    case YVEX_OPERATOR_LANE_API_ONLY: return "API-only";
    case YVEX_OPERATOR_LANE_TEST_ONLY: return "test-only";
    }
    return "unknown";
}
static int descriptor_has_prefix(const yvex_operator_descriptor *descriptor,
                                 size_t count, const char *const *path)
{
    size_t index;
    if (count > descriptor->command_word_count) return 0;
    for (index = 0u; index < count; ++index)
        if (strcmp(path[index], descriptor->command_words[index])) return 0;
    return 1;
}
static void render_leaf_usage(FILE *output,
                              const yvex_operator_descriptor *descriptor)
{
    size_t index;
    char syntax[1024];
    size_t used = (size_t)snprintf(syntax, sizeof(syntax), "usage: yvex%s%s",
        descriptor->command_path[0] ? " " : "", descriptor->command_path);
    for (index = 0u; index < descriptor->argument_count; ++index) {
        const yvex_operator_argument_descriptor *argument = &descriptor->arguments[index];
        int many = !strcmp(argument->multiplicity, "many");
        int written = snprintf(syntax + used, sizeof(syntax) - used, " %s%s%s%s",
            !argument->required || many ? "[" : "", argument->name,
            many ? " ..." : "", !argument->required || many ? "]" : "");
        if (written < 0 || (size_t)written >= sizeof(syntax) - used) return;
        used += (size_t)written;
    }
    if (descriptor->flag_count && used + sizeof(" [options]") <= sizeof(syntax))
        memcpy(syntax + used, " [options]", sizeof(" [options]"));
    (void)yvex_cli_present_text(output, syntax, YVEX_CLI_TEXT_NORMAL, 0u);
}
static const char *flag_value_name(const yvex_operator_flag_descriptor *flag)
{
    if (strcmp(flag->enum_values, "none")) return flag->enum_values;
    if (!strcmp(flag->value_type, "u64")) return "N";
    if (!strcmp(flag->value_type, "number")) return "NUMBER";
    if (!strcmp(flag->value_type, "path")) return "PATH";
    if (!strcmp(flag->value_type, "name")) return "NAME";
    if (!strcmp(flag->value_type, "text")) return "TEXT";
    return "VALUE";
}
static void render_leaf_help(const yvex_operator_descriptor *descriptor)
{
    size_t index;
    yvex_cli_present_field identity[] = {
        {"operation", descriptor->operation_id, YVEX_CLI_TEXT_DIM},
        {"plane", plane_name(descriptor->plane), YVEX_CLI_TEXT_DIM},
        {"visibility", visibility_name(descriptor->visibility), YVEX_CLI_TEXT_DIM},
        {"lane", lane_name(descriptor->lane), YVEX_CLI_TEXT_DIM}};
    render_leaf_usage(stdout, descriptor);
    putchar('\n');
    (void)yvex_cli_present_record(stdout, descriptor->summary, identity, 4u);
    if (strcmp(descriptor->aliases, "none")) {
        const yvex_cli_present_field aliases = {
            "compatibility paths", descriptor->aliases, YVEX_CLI_TEXT_DIM};
        (void)yvex_cli_present_fields(stdout, &aliases, 1u, 2u);
        putchar('\n');
    }
    if (descriptor->argument_count) puts("ARGUMENTS");
    for (index = 0u; index < descriptor->argument_count; ++index) {
        const yvex_operator_argument_descriptor *argument = &descriptor->arguments[index];
        char details[1024];
        yvex_cli_present_field field;
        (void)snprintf(details, sizeof(details), "%s; %s%s%s%s%s",
            argument->required ? "required" : "optional",
            !strcmp(argument->multiplicity, "many") ? "multiple " : "",
            argument->value_type,
            strcmp(argument->enum_values, "none") ? "; values " : "",
            strcmp(argument->enum_values, "none") ? argument->enum_values : "",
            strcmp(argument->range, "delegated") ? "; constrained" : "");
        field = (yvex_cli_present_field){argument->name, details, YVEX_CLI_TEXT_NORMAL};
        (void)yvex_cli_present_fields(stdout, &field, 1u, 2u);
    }
    if (descriptor->flag_count) puts("OPTIONS");
    for (index = 0u; index < descriptor->flag_count; ++index) {
        const yvex_operator_flag_descriptor *flag = &descriptor->flags[index];
        char syntax[384], constraints[768];
        yvex_cli_present_field field;
        snprintf(syntax, sizeof(syntax), "%s%s%s", flag->name,
            flag->takes_value ? " " : "", flag->takes_value ? flag_value_name(flag) : "");
        field = (yvex_cli_present_field){syntax, flag->description, YVEX_CLI_TEXT_NORMAL};
        (void)yvex_cli_present_fields(stdout, &field, 1u, 2u);
        snprintf(constraints, sizeof(constraints), "%s%s%s%s%s%s%s%s%s%s%s",
            !strcmp(flag->multiplicity, "repeatable") ? "repeatable; " : "",
            strcmp(flag->range, "delegated") ? "range " : "",
            strcmp(flag->range, "delegated") ? flag->range : "",
            strcmp(flag->dependencies, "none") ? " requires " : "",
            strcmp(flag->dependencies, "none") ? flag->dependencies : "",
            strcmp(flag->conflicts, "none") ? " conflicts " : "",
            strcmp(flag->conflicts, "none") ? flag->conflicts : "",
            strcmp(flag->aliases, "none") ? " compatibility alias " : "",
            strcmp(flag->aliases, "none") ? flag->aliases : "",
            flag->required ? " required" : "", "");
        if (constraints[0]) (void)yvex_cli_present_text(stdout, constraints, YVEX_CLI_TEXT_DIM, 4u);
    }
}
static void render_command_index_line(const yvex_operator_descriptor *descriptor,
                                      int leaf_only, unsigned int indent)
{
    char syntax[512];
    yvex_cli_present_field field;
    size_t index, used = (size_t)snprintf(syntax, sizeof(syntax), "%s%s",
        leaf_only ? "" : "yvex ", leaf_only
            ? descriptor->command_words[descriptor->command_word_count - 1u] : descriptor->command_path);
    for (index = 0u; index < descriptor->argument_count && used < sizeof(syntax); ++index) {
        const yvex_operator_argument_descriptor *arg = &descriptor->arguments[index];
        int many = !strcmp(arg->multiplicity, "many");
        int written = snprintf(syntax + used, sizeof(syntax) - used, " %s%s%s%s",
            arg->required ? "" : "[", arg->name, many ? " ..." : "", arg->required ? "" : "]");
        if (written < 0 || (size_t)written >= sizeof(syntax) - used) return;
        used += (size_t)written;
    }
    field = (yvex_cli_present_field){syntax, descriptor->summary, YVEX_CLI_TEXT_NORMAL};
    (void)yvex_cli_present_fields(stdout, &field, 1u, indent);
}

static int advanced_group_member(const yvex_operator_descriptor *row, const char *group)
{
    return row->cli_projection && row->command_word_count &&
        (row->visibility == YVEX_OPERATOR_VISIBILITY_PRODUCT_ADVANCED ||
         row->visibility == YVEX_OPERATOR_VISIBILITY_ENGINEERING) && !strcmp(row->help_group, group);
}

static size_t command_parent_size(const yvex_operator_descriptor *row)
{
    const char *space = strrchr(row->command_path, ' ');
    return space ? (size_t)(space - row->command_path) : 0u;
}

static void render_advanced_group(const char *group)
{
    size_t index;
    for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
        const yvex_operator_descriptor *row = &yvex_operator_descriptors[index];
        size_t prior, next, parent_size;
        char parent[512];
        if (!advanced_group_member(row, group)) continue;
        parent_size = command_parent_size(row);
        for (prior = 0u; prior < index; ++prior) {
            const yvex_operator_descriptor *p = &yvex_operator_descriptors[prior];
            if (advanced_group_member(p, group) && command_parent_size(p) == parent_size &&
                !strncmp(p->command_path, row->command_path, parent_size)) break;
        }
        if (prior != index) continue;
        (void)snprintf(parent, sizeof(parent), "yvex%s%.*s", parent_size ? " " : "",
                       (int)parent_size, row->command_path);
        (void)yvex_cli_present_text(stdout, parent, YVEX_CLI_TEXT_STRONG, 2u);
        for (next = index; next < yvex_operator_descriptor_count; ++next) {
            const yvex_operator_descriptor *p = &yvex_operator_descriptors[next];
            if (advanced_group_member(p, group) && command_parent_size(p) == parent_size &&
                !strncmp(p->command_path, row->command_path, parent_size))
                render_command_index_line(p, 1, 4u);
        }
        putchar('\n');
    }
}
void yvex_client_render_usage_error(const yvex_operator_descriptor *operation)
{
    render_leaf_usage(stderr, operation);
}
static void render_discovery_aliases(size_t operation_index)
{
    size_t index;
    int first = 1;
    fputc('[', stdout);
    for (index = 0u; index < yvex_operator_alias_count; ++index) {
        if (yvex_operator_aliases[index].operation_index != operation_index) continue;
        if (!first) fputc(',', stdout);
        yvex_cli_out_json_string(stdout, yvex_operator_aliases[index].path);
        first = 0;
    }
    fputc(']', stdout);
}
static void render_discovery_arguments(
    const yvex_operator_argument_descriptor *arguments, size_t count)
{
    size_t index;
#define JSON_ARGUMENT_FIELD(name, value) \
    do { fputs("\"" name "\":", stdout); yvex_cli_out_json_string(stdout, (value)); } while (0)
    fputc('[', stdout);
    for (index = 0u; index < count; ++index) {
        const yvex_operator_argument_descriptor *argument = &arguments[index];
        if (index) fputc(',', stdout);
        fputc('{', stdout); JSON_ARGUMENT_FIELD("name", argument->name);
        fputc(',', stdout); JSON_ARGUMENT_FIELD("type", argument->value_type);
        printf(",\"required\":%s,", argument->required ? "true" : "false");
        JSON_ARGUMENT_FIELD("multiplicity", argument->multiplicity);
        fputc(',', stdout); JSON_ARGUMENT_FIELD("range", argument->range);
        fputs(",\"enum_values\":", stdout);
        discovery_json_list(stdout, argument->enum_values, '|');
        fputc(',', stdout);
        JSON_ARGUMENT_FIELD("completion_provider", argument->completion_provider);
        fputc(',', stdout);
        JSON_ARGUMENT_FIELD("sensitive_display", argument->sensitive_display);
        fputc(',', stdout); JSON_ARGUMENT_FIELD("validator", argument->validator);
        fputc('}', stdout);
    }
    fputc(']', stdout);
#undef JSON_ARGUMENT_FIELD
}
static void render_discovery_operation(size_t operation_index,
                                       const yvex_operator_descriptor *descriptor)
{
    size_t index;
#define JSON_FIELD(name, value) \
    do { fputs("\"" name "\":", stdout); yvex_cli_out_json_string(stdout, (value)); } while (0)
    fputc('{', stdout);
    printf("\"schema_version\":%u,", descriptor->schema_version);
    JSON_FIELD("operation_id", descriptor->operation_id);
    fputc(',', stdout); JSON_FIELD("command_path", descriptor->command_path);
    fputs(",\"aliases\":", stdout); render_discovery_aliases(operation_index);
    fputc(',', stdout); JSON_FIELD("visibility", visibility_name(descriptor->visibility));
    fputc(',', stdout); JSON_FIELD("plane", plane_name(descriptor->plane));
    fputc(',', stdout); JSON_FIELD("lane", lane_name(descriptor->lane));
    fputc(',', stdout); JSON_FIELD("summary", descriptor->summary);
    fputc(',', stdout); JSON_FIELD("help_group", descriptor->help_group);
    fputc(',', stdout); JSON_FIELD("slash_group", descriptor->slash_group);
    fputs(",\"arguments\":", stdout);
    render_discovery_arguments(descriptor->arguments, descriptor->argument_count);
    fputs(",\"slash_arguments\":", stdout);
    render_discovery_arguments(descriptor->slash_arguments,
                               descriptor->slash_argument_count);
    fputs(",\"flags\":[", stdout);
    for (index = 0u; index < descriptor->flag_count; ++index) {
        const yvex_operator_flag_descriptor *flag = &descriptor->flags[index];
        if (index) fputc(',', stdout);
        fputc('{', stdout); JSON_FIELD("name", flag->name);
        fputs(",\"aliases\":", stdout); discovery_json_list(stdout, flag->aliases, '|');
        fputc(',', stdout); JSON_FIELD("type", flag->value_type);
        fputc(',', stdout); JSON_FIELD("description", flag->description);
        printf(",\"takes_value\":%s,", flag->takes_value ? "true" : "false");
        printf("\"required\":%s,", flag->required ? "true" : "false");
        JSON_FIELD("multiplicity", flag->multiplicity);
        fputc(',', stdout); JSON_FIELD("default_provider", flag->default_provider);
        fputc(',', stdout); JSON_FIELD("range", flag->range);
        fputs(",\"enum_values\":", stdout); discovery_json_list(stdout, flag->enum_values, '|');
        fputs(",\"conflicts\":", stdout); discovery_json_list(stdout, flag->conflicts, '|');
        fputs(",\"dependencies\":", stdout);
        discovery_json_list(stdout, flag->dependencies, '|');
        fputc(',', stdout); JSON_FIELD("environment", flag->environment);
        fputc(',', stdout); JSON_FIELD("config", flag->config);
        fputc(',', stdout); JSON_FIELD("protocol_field", flag->protocol_field);
        fputc(',', stdout); JSON_FIELD("output_interaction", flag->output_interaction);
        fputc(',', stdout); JSON_FIELD("deprecation", flag->deprecation);
        fputc(',', stdout); JSON_FIELD("validator", flag->validator);
        fputc('}', stdout);
    }
    fputs("],\"default_providers\":", stdout);
    discovery_json_list(stdout, descriptor->default_providers, '|');
    fputs(",\"validators\":", stdout);
    discovery_json_list(stdout, descriptor->validator_ids, '|');
    fputs(",\"input_schema\":", stdout); yvex_cli_out_json_string(stdout, descriptor->input_schema);
    fputs(",\"result_schema\":", stdout); yvex_cli_out_json_string(stdout, descriptor->result_schema);
    fputs(",\"side_effects\":", stdout); yvex_cli_out_json_string(stdout, descriptor->side_effects);
    fputs(",\"tty_policy\":", stdout); yvex_cli_out_json_string(stdout, descriptor->tty_policy);
    fputs(",\"requirements\":{", stdout); JSON_FIELD("daemon", descriptor->daemon_requirement);
    fputc(',', stdout); JSON_FIELD("model", descriptor->model_requirement);
    fputc(',', stdout); JSON_FIELD("artifact", descriptor->artifact_requirement);
    fputc(',', stdout); JSON_FIELD("backend", descriptor->backend_requirement);
    fputs("},\"output_schemas\":[", stdout); yvex_cli_out_json_string(stdout, descriptor->result_schema);
    fputs("],\"adapter_id\":", stdout); yvex_cli_out_json_string(stdout, descriptor->adapter_id);
    fputs(",\"renderer_id\":", stdout); yvex_cli_out_json_string(stdout, descriptor->renderer_id);
    fputs(",\"completion_provider\":", stdout);
    yvex_cli_out_json_string(stdout, descriptor->completion_provider);
    fputs(",\"projections\":{\"cli\":", stdout);
    fputs(descriptor->cli_projection ? "true" : "false", stdout);
    fputs(",\"slash\":", stdout); yvex_cli_out_json_string(stdout, descriptor->slash_projection);
    fputs(",\"slash_aliases\":", stdout);
    discovery_json_list(stdout, descriptor->slash_aliases, ',');
    fputs(",\"protocol\":", stdout); yvex_cli_out_json_string(stdout, descriptor->protocol_operation);
    fputs("},\"deprecation\":", stdout); yvex_cli_out_json_string(stdout, descriptor->deprecation_state);
    fputs(",\"superseded_by\":", stdout);
    discovery_json_list(stdout, descriptor->superseded_by, '|');
    fputs(",\"test_owner\":", stdout); yvex_cli_out_json_string(stdout, descriptor->test_owner);
    fputs(",\"documentation_owner\":", stdout);
    yvex_cli_out_json_string(stdout, descriptor->documentation_owner);
    fputc('}', stdout);
#undef JSON_FIELD
}
static void render_discovery_json(void)
{
    size_t index;
    fputs("{\"schema\":", stdout); yvex_cli_out_json_string(stdout, YVEX_COMMAND_DISCOVERY_SCHEMA);
    fputs(",\"registry_identity\":", stdout); yvex_cli_out_json_string(stdout, yvex_operator_registry_identity);
    fputs(",\"build_commit\":", stdout); yvex_cli_out_json_string(stdout, YVEX_BUILD_COMMIT);
    fputs(",\"operations\":[", stdout);
    for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
        if (index) fputc(',', stdout);
        render_discovery_operation(index, &yvex_operator_descriptors[index]);
    }
    fputs("]}\n", stdout);
}

static int root_group(const char *root)
{
    if (!strcmp(root, "chat")) return 0;
    if (!strcmp(root, "serve") || !strcmp(root, "model") ||
        !strcmp(root, "host")) return 1;
    if (!strcmp(root, "inspect")) return 2;
    if (!strcmp(root, "help") || !strcmp(root, "version")) return 3;
    return 4;
}

static int root_first_visible(size_t candidate)
{
    const yvex_operator_descriptor *row = &yvex_operator_descriptors[candidate];
    size_t index;
    if (!row->cli_projection || !row->command_word_count ||
        row->visibility != YVEX_OPERATOR_VISIBILITY_PRODUCT_DEFAULT ||
        root_group(row->command_words[0]) == 4)
        return 0;
    for (index = 0u; index < candidate; ++index) {
        const yvex_operator_descriptor *prior = &yvex_operator_descriptors[index];
        if (prior->cli_projection && prior->command_word_count &&
            prior->visibility == YVEX_OPERATOR_VISIBILITY_PRODUCT_DEFAULT &&
            !strcmp(prior->command_words[0], row->command_words[0]))
            return 0;
    }
    return 1;
}

static const char *root_summary(const char *root)
{
    static const struct { const char *root; const char *summary; } domains[] = {
        {"host", "Inspect and control the foreground host."},
        {"engine", "Load, inspect, and unload engine generations."},
        {"session", "Manage generation-bound conversation state."},
        {"model", "Find, pull, prepare, load, and manage models."},
        {"source", "Acquire, verify, and inspect exact source revisions."},
        {"artifact", "Inspect and verify immutable compiled packages."},
        {"profile", "Inspect durable deployment configurations."},
        {"inspect", "Read bounded system and package evidence."},
        {"bench", "Run bounded component execution and measurement."},
    };
    size_t index;
    for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
        const yvex_operator_descriptor *row = &yvex_operator_descriptors[index];
        if (row->cli_projection && row->command_word_count == 1u &&
            row->visibility != YVEX_OPERATOR_VISIBILITY_REMOVED &&
            !strcmp(row->command_words[0], root))
            return row->summary;
    }
    for (index = 0u; index < sizeof(domains) / sizeof(domains[0]); ++index)
        if (!strcmp(domains[index].root, root)) return domains[index].summary;
    return "Domain operations.";
}

static void render_root_map(void)
{
    static const char *const labels[] = {"USE", "RUNTIME", "TOOLS", "META"};
    size_t group, index;
    (void)yvex_cli_present_text(stdout, "YVEX native model execution", YVEX_CLI_TEXT_STRONG, 0u);
    for (group = 0u; group < sizeof(labels) / sizeof(labels[0]); ++group) {
        yvex_cli_present_field fields[16];
        size_t count = 0u;
        for (index = 0u; index < yvex_operator_descriptor_count && count < 16u; ++index) {
            const yvex_operator_descriptor *row = &yvex_operator_descriptors[index];
            if (root_first_visible(index) && root_group(row->command_words[0]) == (int)group)
                fields[count++] = (yvex_cli_present_field){row->command_words[0],
                    root_summary(row->command_words[0]), YVEX_CLI_TEXT_NORMAL};
        }
        (void)yvex_cli_present_record(stdout, labels[group], fields, count);
    }
    (void)yvex_cli_present_text(stdout, "Use `yvex help COMMAND` for details.", YVEX_CLI_TEXT_DIM, 0u);
}

static void render_product_grammar(void)
{
    (void)yvex_cli_present_text(stdout, "LIFECYCLE", YVEX_CLI_TEXT_STRONG, 0u);
    (void)yvex_cli_present_text(stdout,
        "model search -> model pull -> model prepare -> serve -> model load -> chat",
        YVEX_CLI_TEXT_NORMAL, 2u);
    (void)yvex_cli_present_text(stdout, "model push distributes; model unload changes runtime residency.",
        YVEX_CLI_TEXT_DIM, 2u);
    puts("");
    (void)yvex_cli_present_text(stdout, "READ", YVEX_CLI_TEXT_STRONG, 0u);
    (void)yvex_cli_present_text(stdout, "yvex model list / show / active\nyvex host status / memory / logs",
        YVEX_CLI_TEXT_NORMAL, 2u);
    puts("");
    (void)yvex_cli_present_text(stdout,
        "Use `yvex help model` or `yvex help host` to explore their operations.", YVEX_CLI_TEXT_DIM, 0u);
}

int yvex_client_render_help_path(size_t path_count, const char *const *path,
                                 int advanced, int json)
{
    const yvex_operator_descriptor *exact = NULL;
    size_t index, matches = 0u;
    if (json) {
        render_discovery_json();
        return 0;
    }
    if (!path_count) {
        render_root_map();
        render_product_grammar();
        if (advanced) {
            puts("\nADVANCED AND ENGINEERING\n");
            for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
                const yvex_operator_descriptor *descriptor =
                    &yvex_operator_descriptors[index];
                if (descriptor->cli_projection &&
                    (descriptor->visibility == YVEX_OPERATOR_VISIBILITY_PRODUCT_ADVANCED ||
                     descriptor->visibility == YVEX_OPERATOR_VISIBILITY_ENGINEERING))
                    {
                    size_t prior;
                    for (prior = 0u; prior < index; ++prior) {
                        const yvex_operator_descriptor *p = &yvex_operator_descriptors[prior];
                        if (p->cli_projection &&
                            (p->visibility == YVEX_OPERATOR_VISIBILITY_PRODUCT_ADVANCED ||
                             p->visibility == YVEX_OPERATOR_VISIBILITY_ENGINEERING) &&
                            !strcmp(p->help_group, descriptor->help_group)) break;
                    }
                    if (prior == index) {
                        (void)yvex_cli_present_text(stdout, descriptor->help_group, YVEX_CLI_TEXT_STRONG, 0u);
                        render_advanced_group(descriptor->help_group);
                        putchar('\n');
                    }
                }
            }
        } else {
            (void)yvex_cli_present_text(stdout,
                "Use `yvex help --advanced` for advanced and engineering commands.", YVEX_CLI_TEXT_DIM, 0u);
        }
        return 0;
    }
    for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
        const yvex_operator_descriptor *descriptor = &yvex_operator_descriptors[index];
        if (!descriptor->cli_projection || !descriptor_has_prefix(descriptor, path_count, path))
            continue;
        if (descriptor->command_word_count == path_count) exact = descriptor;
        matches++;
    }
    if (!matches) {
        fprintf(stderr, "yvex: unknown help path");
        for (index = 0u; index < path_count; ++index) fprintf(stderr, " %s", path[index]);
        fputc('\n', stderr);
        return 2;
    }
    if (exact) {
        render_leaf_help(exact);
        if (matches == 1u) return 0;
        puts("\nsubcommands:");
    } else {
        puts(path_count ? "YVEX command namespace\n" : "YVEX local inference\n");
    }
    for (index = 0u; index < yvex_operator_descriptor_count; ++index) {
        const yvex_operator_descriptor *descriptor = &yvex_operator_descriptors[index];
        int visible = path_count != 0u ||
                      descriptor->visibility == YVEX_OPERATOR_VISIBILITY_PRODUCT_DEFAULT ||
                      (advanced && (descriptor->visibility == YVEX_OPERATOR_VISIBILITY_PRODUCT_ADVANCED ||
                                    descriptor->visibility == YVEX_OPERATOR_VISIBILITY_ENGINEERING));
        if (descriptor != exact && descriptor->cli_projection && visible &&
            descriptor_has_prefix(descriptor, path_count, path))
            render_command_index_line(descriptor, 0, 2u);
    }
    if (!advanced) puts("\nUse `yvex help --advanced` for advanced and engineering commands.");
    return 0;
}
void yvex_cli_json_begin(FILE *fp) {
    yvex_cli_out_line(fp, "{");
}
void yvex_cli_json_end(FILE *fp) {
    yvex_cli_out_line(fp, "}");
}

static int json_field(FILE *fp, const char *key, yvex_cli_field_kind kind, const void *value,
                      int comma) {
    (void)yvex_cli_out_puts(fp, "  ");
    yvex_cli_out_json_string(fp, key);
    (void)yvex_cli_out_puts(fp, ": ");
    switch (kind) {
    case YVEX_CLI_FIELD_TEXT:
    case YVEX_CLI_FIELD_TEXT_ARRAY:
        yvex_cli_out_json_string(fp, value);
        break;
    case YVEX_CLI_FIELD_U64:
        (void)yvex_cli_out_writef(fp, "%llu", *(const unsigned long long *)value);
        break;
    case YVEX_CLI_FIELD_U32:
        (void)yvex_cli_out_writef(fp, "%u", *(const unsigned int *)value);
        break;
    case YVEX_CLI_FIELD_I32:
        (void)yvex_cli_out_writef(fp, "%d", *(const int *)value);
        break;
    case YVEX_CLI_FIELD_BOOL:
        (void)yvex_cli_out_puts(fp, *(const int *)value ? "true" : "false");
        break;
    case YVEX_CLI_FIELD_DOUBLE:
        if (isfinite(*(const double *)value))
            (void)yvex_cli_out_writef(fp, "%.17g", *(const double *)value);
        else
            (void)yvex_cli_out_puts(fp, "null");
        break;
    default:
        return YVEX_ERR_UNSUPPORTED;
    }
    (void)yvex_cli_out_writef(fp, "%s\n", comma ? "," : "");
    return ferror(fp) ? YVEX_ERR_IO : YVEX_OK;
}

void yvex_cli_json_field_str(FILE *fp, const char *key, const char *value, int comma) {
    (void)json_field(fp, key, YVEX_CLI_FIELD_TEXT_ARRAY, value ? value : "", comma);
}

void yvex_cli_json_field_u64(FILE *fp, const char *key, unsigned long long value, int comma) {
    (void)json_field(fp, key, YVEX_CLI_FIELD_U64, &value, comma);
}

void yvex_cli_json_field_bool(FILE *fp, const char *key, int value, int comma) {
    (void)json_field(fp, key, YVEX_CLI_FIELD_BOOL, &value, comma);
}

int yvex_cli_json_fields(FILE *fp, const void *object, const yvex_cli_field_spec *fields,
                         size_t field_count, int comma) {
    const unsigned char *base = object;
    size_t index;

    if (!fp || !object || (!fields && field_count))
        return YVEX_ERR_INVALID_ARG;
    for (index = 0; index < field_count; ++index) {
        const yvex_cli_field_spec *field = &fields[index];
        const void *value = base + field->offset;
        int separator = comma || index + 1u < field_count;
        if (field->kind == YVEX_CLI_FIELD_TEXT)
            value = *(const char *const *)value;
        if (json_field(fp, field->key, field->kind, value, separator) != YVEX_OK)
            return ferror(fp) ? YVEX_ERR_IO : YVEX_ERR_UNSUPPORTED;
    }
    return ferror(fp) ? YVEX_ERR_IO : YVEX_OK;
}
