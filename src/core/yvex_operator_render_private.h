/*
 * yvex_operator_render_private.h - Private operator text rendering helpers.
 *
 * This header owns small reusable text rendering primitives for existing
 * operator report surfaces. It is not a CLI command file, not a public API,
 * and does not implement JSON, color, or command behavior.
 */
#ifndef YVEX_OPERATOR_RENDER_PRIVATE_H
#define YVEX_OPERATOR_RENDER_PRIVATE_H

#include <stdio.h>

typedef enum {
    YVEX_RENDER_MODE_PORCELAIN = 0,
    YVEX_RENDER_MODE_TABLE,
    YVEX_RENDER_MODE_AUDIT
} yvex_render_mode;

typedef struct {
    FILE *fp;
    yvex_render_mode mode;
} yvex_render_out;

static inline void yvex_render_out_init(yvex_render_out *out,
                                        FILE *fp,
                                        yvex_render_mode mode)
{
    if (!out) {
        return;
    }
    out->fp = fp ? fp : stdout;
    out->mode = mode;
}

static inline FILE *yvex_render_fp(const yvex_render_out *out)
{
    return out && out->fp ? out->fp : stdout;
}

static inline const char *yvex_render_text(const char *text)
{
    return text && text[0] ? text : "unknown";
}

static inline void yvex_render_report_title(const yvex_render_out *out,
                                            const char *report,
                                            const char *subject,
                                            const char *state)
{
    FILE *fp = yvex_render_fp(out);

    fprintf(fp, "%s: %s", yvex_render_text(report), yvex_render_text(subject));
    if (state && state[0]) {
        fprintf(fp, " [%s]", state);
    }
    fputc('\n', fp);
}

static inline void yvex_render_kv(const yvex_render_out *out,
                                  const char *key,
                                  const char *value)
{
    fprintf(yvex_render_fp(out), "%s: %s\n",
            yvex_render_text(key),
            yvex_render_text(value));
}

static inline void yvex_render_kv_u(const yvex_render_out *out,
                                    const char *key,
                                    unsigned int value)
{
    fprintf(yvex_render_fp(out), "%s: %u\n", yvex_render_text(key), value);
}

static inline void yvex_render_status(const yvex_render_out *out,
                                      const char *value)
{
    yvex_render_kv(out, "status", value);
}

static inline void yvex_render_top_blocker(const yvex_render_out *out,
                                           const char *value)
{
    yvex_render_kv(out, "top_blocker", value);
}

static inline void yvex_render_next(const yvex_render_out *out,
                                    const char *value)
{
    yvex_render_kv(out, "next", value);
}

static inline void yvex_render_boundary(const yvex_render_out *out,
                                        const char *value)
{
    yvex_render_kv(out, "boundary", value);
}

static inline void yvex_render_section(const yvex_render_out *out,
                                       const char *title)
{
    fprintf(yvex_render_fp(out), "%s\n", yvex_render_text(title));
}

static inline void yvex_render_fields2(const yvex_render_out *out,
                                       const char *key0,
                                       const char *value0,
                                       const char *key1,
                                       const char *value1)
{
    fprintf(yvex_render_fp(out), "%s: %s  %s: %s\n",
            yvex_render_text(key0),
            yvex_render_text(value0),
            yvex_render_text(key1),
            yvex_render_text(value1));
}

static inline void yvex_render_fields3(const yvex_render_out *out,
                                       const char *key0,
                                       const char *value0,
                                       const char *key1,
                                       const char *value1,
                                       const char *key2,
                                       const char *value2)
{
    fprintf(yvex_render_fp(out), "%s: %s  %s: %s  %s: %s\n",
            yvex_render_text(key0),
            yvex_render_text(value0),
            yvex_render_text(key1),
            yvex_render_text(value1),
            yvex_render_text(key2),
            yvex_render_text(value2));
}

static inline void yvex_render_table_header(const yvex_render_out *out,
                                            const char *header)
{
    fprintf(yvex_render_fp(out), "%s\n", yvex_render_text(header));
}

static inline void yvex_render_table_row(const yvex_render_out *out,
                                         const char *row)
{
    fprintf(yvex_render_fp(out), "%s\n", yvex_render_text(row));
}

#endif /* YVEX_OPERATOR_RENDER_PRIVATE_H */
