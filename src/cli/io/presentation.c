/* Adapt bounded human intent to the independently qualified terminal producer.
 * No model facts, JSON policy, editor lifetime or terminal geometry live here. */
#include "src/cli/io/private.h"
#include <yvex/internal/cli_presentation.h>
#include <yvex/internal/cli_table.h>
#include <replai.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static char role_sequences[7][64];
static pthread_once_t roles_once = PTHREAD_ONCE_INIT;

static void initialize_roles(void)
{
    uint32_t role;
    for (role = 0u; role < 7u; ++role) {
        size_t bytes = 0u;
        if (replai_role_sequence(role, 1u, (uint8_t *)role_sequences[role],
                                sizeof(role_sequences[role]) - 1u, &bytes) != REPLAI_OK)
            role_sequences[role][0] = '\0';
        else role_sequences[role][bytes] = '\0';
    }
}

const char *yvex_cli_present_style(yvex_cli_text_role role)
{
    (void)pthread_once(&roles_once, initialize_roles);
    return role >= YVEX_CLI_TEXT_NORMAL && role <= YVEX_CLI_TEXT_ERROR
               ? role_sequences[role] : "";
}

size_t yvex_cli_present_cells(const char *text, size_t bytes)
{
    size_t cells = 0u;
    return replai_text_cells((const uint8_t *)text, bytes, &cells) == REPLAI_OK
               ? cells : 0u;
}

static replai_text text_value(replai_span *span, const char *value,
                               yvex_cli_text_role role)
{
    *span = (replai_span){.struct_size = sizeof(*span),
        .extension_version = REPLAI_PRESENTATION_VERSION,
        .text = (const uint8_t *)(value ? value : ""),
        .text_bytes = strlen(value ? value : ""), .role = (uint32_t)role};
    return (replai_text){.struct_size = sizeof(replai_text),
        .extension_version = REPLAI_PRESENTATION_VERSION, .spans = span, .span_count = 1u};
}

static int render_blocks(FILE *stream, const replai_block *blocks, size_t count,
                         unsigned int indent, unsigned int width)
{
    yvex_cli_terminal_style style;
    replai_render render = {.struct_size = sizeof(render),
        .extension_version = REPLAI_PRESENTATION_VERSION,
        .columns = width, .indent = indent};
    uint8_t *output;
    size_t bytes = 0u;
    replai_status status;
    if (!stream) return YVEX_ERR_INVALID_ARG;
    yvex_cli_terminal_style_get(stream, &style);
    render.styled = style.strong[0] != '\0';
    status = replai_document_render(blocks, count, &render, NULL, 0u, &bytes);
    if (status != REPLAI_OK) return YVEX_ERR_INVALID_ARG;
    output = malloc(bytes ? bytes : 1u);
    if (!output) return YVEX_ERR_NOMEM;
    status = replai_document_render(blocks, count, &render, output, bytes, &bytes);
    if (status == REPLAI_OK && fwrite(output, 1u, bytes, stream) != bytes)
        status = REPLAI_IO;
    free(output);
    return status == REPLAI_OK ? YVEX_OK : YVEX_ERR_IO;
}

int yvex_cli_present_text(FILE *stream, const char *text,
                          yvex_cli_text_role role, unsigned int indent)
{
    replai_span span;
    replai_block block = {.struct_size = sizeof(block),
        .extension_version = REPLAI_PRESENTATION_VERSION, .kind = REPLAI_BLOCK_PARAGRAPH};
    block.text = text_value(&span, text, role);
    block.label = (replai_text){.struct_size = sizeof(replai_text),
        .extension_version = REPLAI_PRESENTATION_VERSION};
    return render_blocks(stream, &block, 1u, indent, yvex_cli_terminal_columns(stream));
}

int yvex_cli_present_fields_width(FILE *stream, const yvex_cli_present_field *fields,
                                  size_t count, unsigned int indent, unsigned int width)
{
    replai_block *blocks;
    replai_span *spans;
    size_t index;
    int status;
    if (!stream || (count && !fields) || count > 4096u) return YVEX_ERR_INVALID_ARG;
    blocks = calloc(count ? count : 1u, sizeof(*blocks));
    spans = calloc(count ? count * 2u : 1u, sizeof(*spans));
    if (!blocks || !spans) { free(blocks); free(spans); return YVEX_ERR_NOMEM; }
    for (index = 0u; index < count; ++index) {
        blocks[index] = (replai_block){.struct_size = sizeof(replai_block),
            .extension_version = REPLAI_PRESENTATION_VERSION, .kind = REPLAI_BLOCK_FIELD};
        blocks[index].text = text_value(&spans[index * 2u], fields[index].value, fields[index].role);
        blocks[index].label = text_value(&spans[index * 2u + 1u], fields[index].label,
                                         YVEX_CLI_TEXT_DIM);
    }
    status = render_blocks(stream, blocks, count, indent, width);
    free(spans);
    free(blocks);
    return status;
}

int yvex_cli_present_fields(FILE *stream, const yvex_cli_present_field *fields,
                            size_t count, unsigned int indent)
{
    return yvex_cli_present_fields_width(stream, fields, count, indent,
                                         yvex_cli_terminal_columns(stream));
}

int yvex_cli_present_record(FILE *stream, const char *name,
                            const yvex_cli_present_field *fields, size_t count)
{
    int rc = yvex_cli_present_text(stream, name, YVEX_CLI_TEXT_STRONG, 0u);
    if (rc == YVEX_OK) rc = yvex_cli_present_fields(stream, fields, count, 2u);
    if (rc == YVEX_OK && yvex_cli_out_char(stream, '\n') < 0) rc = YVEX_ERR_IO;
    return rc;
}
