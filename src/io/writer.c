/* Owner: src/io.
 * Owns: escaped JSON byte emission to explicit file streams.
 * Does not own: operator output, CLI rendering, domain facts, runtime, generation, eval, or benchmark.
 * Invariants: writes only to caller-provided FILE handles and never chooses standard streams.
 * Boundary: JSON serialization is not source verification or runtime readiness.
 * Purpose: centralize JSON string escaping for file-backed serializers.
 * Inputs: caller-owned writable streams and immutable byte strings.
 * Effects: appends encoded JSON text to the supplied stream.
 * Failure: stdio records write failures for the owning serializer to detect. */
#include <yvex/internal/io.h>

/* Purpose: append one quoted and escaped JSON string value.
 * Inputs: a writable stream and a nullable string; NULL denotes an empty value.
 * Effects: advances the caller-owned stream without changing its lifecycle.
 * Failure: stdio retains any underlying write failure.
 * Boundary: serialization primitive; it neither opens nor closes the stream. */
void yvex_file_json_write_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
        case '"':
            fputs("\\\"", fp);
            break;
        case '\\':
            fputs("\\\\", fp);
            break;
        case '\b':
            fputs("\\b", fp);
            break;
        case '\f':
            fputs("\\f", fp);
            break;
        case '\n':
            fputs("\\n", fp);
            break;
        case '\r':
            fputs("\\r", fp);
            break;
        case '\t':
            fputs("\\t", fp);
            break;
        default:
            if (ch < 32) {
                fprintf(fp, "\\u%04x", (unsigned int)ch);
            } else {
                fputc((int)ch, fp);
            }
            break;
        }
    }
    fputc('"', fp);
}

/* Purpose: append one named JSON string field with explicit indentation and comma policy.
 * Inputs: a writable stream, field formatting, name, value, and trailing-comma flag.
 * Effects: writes one field to the caller-owned stream.
 * Failure: stdio retains any underlying write failure.
 * Boundary: object framing and stream lifecycle remain caller responsibilities. */
void yvex_file_json_write_field(FILE *fp,
                                const char *indent,
                                const char *name,
                                const char *value,
                                int comma)
{
    fputs(indent ? indent : "", fp);
    yvex_file_json_write_string(fp, name);
    fputs(": ", fp);
    yvex_file_json_write_string(fp, value);
    fputs(comma ? ",\n" : "\n", fp);
}
