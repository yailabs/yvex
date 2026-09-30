/* Human presentation intent. Domain facts and machine serializers are separate. */
#ifndef INCLUDE_YVEX_INTERNAL_CLI_PRESENTATION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_CLI_PRESENTATION_H_INCLUDED
#include <stddef.h>
#include <stdio.h>
#include <yvex/server.h>

typedef struct {
    const char *reset, *strong, *accent, *dim, *success, *warning, *error;
} yvex_cli_terminal_style;
typedef enum {
    YVEX_CLI_STREAM_STYLE_NORMAL = 0,
    YVEX_CLI_STREAM_STYLE_DIM,
    YVEX_CLI_STREAM_STYLE_ACCENT,
    YVEX_CLI_STREAM_STYLE_STRONG
} yvex_cli_stream_style;
#define YVEX_CLI_STREAM_LINE_CAP 16384u
typedef struct {
    FILE *output;
    yvex_cli_terminal_style style;
    yvex_client_stream_channel channel;
    yvex_cli_stream_style active_style, line_style;
    unsigned char line[YVEX_CLI_STREAM_LINE_CAP], inline_previous;
    unsigned char geometry[YVEX_CLI_STREAM_LINE_CAP];
    size_t line_count, geometry_count;
    unsigned int column, prose_width, line_indent, inline_flags;
    int enhanced, in_fence, pending_cr, channel_announced, line_started;
    int wrote_bytes, last_newline, pending_space;
} yvex_cli_stream_renderer;

typedef enum {
    YVEX_CLI_TEXT_NORMAL, YVEX_CLI_TEXT_STRONG, YVEX_CLI_TEXT_ACCENT,
    YVEX_CLI_TEXT_DIM, YVEX_CLI_TEXT_SUCCESS, YVEX_CLI_TEXT_WARNING,
    YVEX_CLI_TEXT_ERROR
} yvex_cli_text_role;

typedef struct {
    const char *label;
    const char *value;
    yvex_cli_text_role role;
} yvex_cli_present_field;

int yvex_cli_present_text(FILE *stream, const char *text,
                          yvex_cli_text_role role, unsigned int indent);
int yvex_cli_present_fields(FILE *stream, const yvex_cli_present_field *fields,
                            size_t count, unsigned int indent);
int yvex_cli_present_fields_width(FILE *stream, const yvex_cli_present_field *fields,
                                  size_t count, unsigned int indent, unsigned int width);
int yvex_cli_present_record(FILE *stream, const char *name,
                            const yvex_cli_present_field *fields, size_t count);
const char *yvex_cli_present_style(yvex_cli_text_role role);
size_t yvex_cli_present_cells(const char *text, size_t bytes);
#endif
