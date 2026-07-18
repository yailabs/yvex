/*
 * Owner: cli.render.private (cli.render).
 * Owns: the private-interface boundary consumed by CLI renderers.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: private-interface; moving this contract requires an ownership-manifest change.
 *
 * render.h - Private operator text rendering helpers.
 *
 * This header owns small reusable text rendering primitives for existing
 * operator report surfaces. It is not a CLI command file, not a public API,
 * and does not implement JSON, color, or command behavior.
 */
#ifndef YVEX_OPERATOR_RENDER_PRIVATE_H
#define YVEX_OPERATOR_RENDER_PRIVATE_H

#include <stdio.h>
#include "src/cli/io/out.h"

typedef enum {
    YVEX_RENDER_MODE_PORCELAIN = 0,
    YVEX_RENDER_MODE_TABLE,
    YVEX_RENDER_MODE_AUDIT
} yvex_render_mode;

typedef struct {
    FILE *fp;
    yvex_render_mode mode;
} yvex_render_out;

static inline void render_out_init(yvex_render_out *out,
                                        FILE *fp,
                                        yvex_render_mode mode)
{
    if (!out) {
        return;
    }
    out->fp = fp ? fp : stdout;
    out->mode = mode;
}

static inline FILE *render_fp(const yvex_render_out *out)
{
    return out && out->fp ? out->fp : stdout;
}

static inline const char *render_text(const char *text)
{
    return text && text[0] ? text : "unknown";
}

static inline void render_report_title(const yvex_render_out *out,
                                            const char *report,
                                            const char *subject,
                                            const char *state)
{
    FILE *fp = render_fp(out);

    yvex_cli_out_writef(fp, "%s: %s", render_text(report), render_text(subject));
    if (state && state[0]) {
        yvex_cli_out_writef(fp, " [%s]", state);
    }
    yvex_cli_out_writef(fp, "\n");
}

static inline void render_kv(const yvex_render_out *out,
                                  const char *key,
                                  const char *value)
{
    yvex_cli_out_writef(render_fp(out), "%s: %s\n",
            render_text(key),
            render_text(value));
}

static inline void render_kv_u(const yvex_render_out *out,
                                    const char *key,
                                    unsigned int value)
{
    yvex_cli_out_writef(render_fp(out), "%s: %u\n", render_text(key), value);
}

static inline void render_status(const yvex_render_out *out,
                                      const char *value)
{
    render_kv(out, "status", value);
}

static inline void render_top_blocker(const yvex_render_out *out,
                                           const char *value)
{
    render_kv(out, "top_blocker", value);
}

static inline void render_next(const yvex_render_out *out,
                                    const char *value)
{
    render_kv(out, "next", value);
}

static inline void render_boundary(const yvex_render_out *out,
                                        const char *value)
{
    render_kv(out, "boundary", value);
}

static inline void render_section(const yvex_render_out *out,
                                       const char *title)
{
    yvex_cli_out_writef(render_fp(out), "%s\n", render_text(title));
}

static inline void render_fields2(const yvex_render_out *out,
                                       const char *key0,
                                       const char *value0,
                                       const char *key1,
                                       const char *value1)
{
    yvex_cli_out_writef(render_fp(out), "%s: %s  %s: %s\n",
            render_text(key0),
            render_text(value0),
            render_text(key1),
            render_text(value1));
}

static inline void render_fields3(const yvex_render_out *out,
                                       const char *key0,
                                       const char *value0,
                                       const char *key1,
                                       const char *value1,
                                       const char *key2,
                                       const char *value2)
{
    yvex_cli_out_writef(render_fp(out), "%s: %s  %s: %s  %s: %s\n",
            render_text(key0),
            render_text(value0),
            render_text(key1),
            render_text(value1),
            render_text(key2),
            render_text(value2));
}

static inline void render_table_header(const yvex_render_out *out,
                                            const char *header)
{
    yvex_cli_out_writef(render_fp(out), "%s\n", render_text(header));
}

static inline void render_table_row(const yvex_render_out *out,
                                         const char *row)
{
    yvex_cli_out_writef(render_fp(out), "%s\n", render_text(row));
}

#endif /* YVEX_OPERATOR_RENDER_PRIVATE_H */
