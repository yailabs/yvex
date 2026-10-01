/* Render bounded, line-oriented CLI tables without terminal ownership. */
#define _POSIX_C_SOURCE 200809L
#include "src/cli/io/private.h"
#include "src/cli/io/terminal/private.h"
#include <yvex/internal/cli_table.h>

#include <yvex/internal/cli_presentation.h>
#include <replai.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void yvex_cli_precision_format(char *out, size_t capacity, const char *raw)
{
    static const struct { const char *raw, *display; } names[] = {
        {"iq2xxs-q2k-mxfp4", "IQ2_XXS/Q2_K/MXFP4"},
        {"iq2xxs", "IQ2_XXS"},
        {"iq2_xxs", "IQ2_XXS"},
        {"q2k", "Q2_K"},
        {"q2_k", "Q2_K"},
        {"mxfp4", "MXFP4"},
        {"bf16", "BF16"},
        {"f16", "FP16"},
        {"fp16", "FP16"},
        {"f32", "FP32"},
        {"fp32", "FP32"}
    };
    size_t index;
    if (!out || !capacity) return;
    for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index)
        if (raw && !strcasecmp(raw, names[index].raw)) {
            snprintf(out, capacity, "%s", names[index].display);
            return;
        }
    snprintf(out, capacity, "%s", raw && raw[0] ? raw : "not recorded");
}

unsigned int yvex_cli_terminal_columns(FILE *fp)
{
    unsigned int width;
    const char *configured = getenv("COLUMNS");
    char *end = NULL;
    unsigned long value;

    if (configured && configured[0]) {
        errno = 0;
        value = strtoul(configured, &end, 10);
        if (!errno && end && !*end && value >= 8u && value <= 4096u)
            return (unsigned int)value;
    }
    width = yvex_cli_terminal_interactive(fp) ? yvex_cli_terminal_width(fp) : 0u;
    if (width >= 8u) return width;
    return 120u;
}

static uint32_t table_role(yvex_cli_table_tone tone)
{
    static const uint32_t roles[] = {REPLAI_ROLE_DEFAULT, REPLAI_ROLE_ACCENT,
        REPLAI_ROLE_SUCCESS, REPLAI_ROLE_WARNING, REPLAI_ROLE_ERROR, REPLAI_ROLE_DIM};
    return (unsigned int)tone < sizeof(roles) / sizeof(roles[0]) ? roles[tone] : REPLAI_ROLE_DEFAULT;
}

static int table_records(FILE *fp, const yvex_cli_table_column *columns, size_t column_count,
                          const yvex_cli_table_row *rows, size_t row_count, unsigned int width)
{
    size_t row, column;
    for (row = 0u; row < row_count; ++row) {
        yvex_cli_present_field fields[17];
        size_t count = column_count;
        int rc;
        for (column = 0u; column < column_count; ++column)
            fields[column] = (yvex_cli_present_field){columns[column].heading,
                rows[row].cells[column].text,
                (yvex_cli_text_role)table_role(rows[row].cells[column].tone)};
        if (rows[row].secondary && rows[row].secondary[0])
            fields[count++] = (yvex_cli_present_field){"detail", rows[row].secondary, YVEX_CLI_TEXT_DIM};
        rc = yvex_cli_present_fields_width(fp, fields, count, 2u, width);
        if (rc != YVEX_OK) return rc;
        if (yvex_cli_out_char(fp, '\n') < 0) return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_cli_table_render_width(FILE *fp, const yvex_cli_table_column *columns,
                                size_t column_count, const yvex_cli_table_row *rows,
                                size_t row_count, unsigned int width)
{
    replai_block *blocks = NULL;
    replai_span *spans = NULL;
    replai_text *cells = NULL;
    uint8_t *output = NULL;
    yvex_cli_terminal_style style;
    size_t row, column, count, bytes = 0u;
    uint32_t alignment = 0u;
    int has_secondary = 0;
    replai_status status;
    replai_render render = {.struct_size = sizeof(render),
        .extension_version = REPLAI_PRESENTATION_VERSION, .columns = width};
    if (!fp || !columns || !column_count || column_count > 16u ||
        (row_count && !rows) || row_count >= 4096u || width < 8u || width > 4096u)
        return YVEX_ERR_INVALID_ARG;
    for (row = 0u; row < row_count; ++row) {
        if (!rows[row].cells) return YVEX_ERR_INVALID_ARG;
        if (rows[row].secondary && rows[row].secondary[0])
            has_secondary = 1;
    }
    if (has_secondary) return table_records(fp, columns, column_count, rows, row_count, width);
    count = (row_count + 1u) * column_count;
    blocks = calloc(row_count + 1u, sizeof(*blocks));
    spans = calloc(count, sizeof(*spans));
    cells = calloc(count, sizeof(*cells));
    if (!blocks || !spans || !cells) { status = REPLAI_CAPACITY; goto done; }
    for (column = 0u; column < column_count; ++column)
        if (columns[column].alignment == YVEX_CLI_TABLE_RIGHT) alignment |= 1u << column;
    for (row = 0u; row <= row_count; ++row) {
        blocks[row] = (replai_block){.struct_size = sizeof(replai_block),
            .extension_version = REPLAI_PRESENTATION_VERSION,
            .kind = row ? REPLAI_BLOCK_TABLE_ROW : REPLAI_BLOCK_TABLE_HEADER,
            .level = row ? 0u : alignment,
            .text = {.struct_size = sizeof(replai_text), .extension_version = REPLAI_PRESENTATION_VERSION},
            .label = {.struct_size = sizeof(replai_text), .extension_version = REPLAI_PRESENTATION_VERSION},
            .cells = cells + row * column_count, .cell_count = column_count};
        for (column = 0u; column < column_count; ++column) {
            size_t index = row * column_count + column;
            const char *text = row ? rows[row - 1u].cells[column].text : columns[column].heading;
            text = text ? text : "";
            spans[index] = (replai_span){.struct_size = sizeof(replai_span),
                .extension_version = REPLAI_PRESENTATION_VERSION,
                .text = (const uint8_t *)text, .text_bytes = strlen(text),
                .role = row ? table_role(rows[row - 1u].cells[column].tone) : REPLAI_ROLE_STRONG};
            cells[index] = (replai_text){.struct_size = sizeof(replai_text),
                .extension_version = REPLAI_PRESENTATION_VERSION, .spans = spans + index, .span_count = 1u};
        }
    }
    yvex_cli_terminal_style_get(fp, &style);
    render.styled = style.strong[0] != '\0';
    status = replai_document_render(blocks, row_count + 1u, &render, NULL, 0u, &bytes);
    if (status != REPLAI_OK) goto done;
    output = malloc(bytes ? bytes : 1u);
    if (!output) { status = REPLAI_CAPACITY; goto done; }
    status = replai_document_render(blocks, row_count + 1u, &render, output, bytes, &bytes);
    if (status == REPLAI_OK && fwrite(output, 1u, bytes, fp) != bytes) status = REPLAI_IO;
done:
    free(output); free(cells); free(spans); free(blocks);
    return status == REPLAI_OK ? YVEX_OK : status == REPLAI_CAPACITY ? YVEX_ERR_NOMEM
               : status == REPLAI_IO ? YVEX_ERR_IO : YVEX_ERR_INVALID_ARG;
}

int yvex_cli_table_render(FILE *fp, const yvex_cli_table_column *columns,
                          size_t column_count, const yvex_cli_table_row *rows, size_t row_count)
{
    return yvex_cli_table_render_width(fp, columns, column_count, rows, row_count,
                                       yvex_cli_terminal_columns(fp));
}
