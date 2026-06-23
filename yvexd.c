/*
 * YVEX - yvexd server binary
 *
 *
 * Purpose:
 *   Provides the server shell standalone yvexd process. It serves local health, metrics,
 *   and model-catalog status endpoints. It does not generate model output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/yvex.h>

static void print_help(FILE *fp)
{
    fprintf(fp, "usage: yvexd [--host HOST] [--port PORT] [--model FILE] [--backend cpu|cuda] [--one-request]\n");
    fprintf(fp, "\n");
    fprintf(fp, "Starts the server shell local server shell. Endpoints: /health, /metrics, /v1/models.\n");
    fprintf(fp, "Generation endpoints are not implemented in server shell.\n");
}

static int parse_port(const char *text, unsigned int *out)
{
    char *end = NULL;
    unsigned long value;

    if (!text || !out) {
        return 0;
    }
    value = strtoul(text, &end, 10);
    if (!end || *end != '\0' || value == 0 || value > 65535ul) {
        return 0;
    }
    *out = (unsigned int)value;
    return 1;
}

static int print_error(const yvex_error *err, int exit_code)
{
    fprintf(stderr, "yvexd: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

int main(int argc, char **argv)
{
    yvex_server *server = NULL;
    yvex_server_options options;
    yvex_server_summary summary;
    yvex_error err;
    int i;
    int rc;

    memset(&options, 0, sizeof(options));
    options.host = "127.0.0.1";
    options.port = 8080;
    options.backend_name = "cpu";

    yvex_error_clear(&err);

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(stdout);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("%s\n", yvex_version_string());
            return 0;
        } else if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvexd: --host requires a value\n");
                return 2;
            }
            options.host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc || !parse_port(argv[i + 1], &options.port)) {
                fprintf(stderr, "yvexd: --port must be in range 1..65535\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvexd: --model requires a value\n");
                return 2;
            }
            options.model_path = argv[++i];
            options.load_engine = 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvexd: --backend requires a value\n");
                return 2;
            }
            options.backend_name = argv[++i];
        } else if (strcmp(argv[i], "--one-request") == 0) {
            options.one_request = 1;
        } else {
            fprintf(stderr, "yvexd: unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    rc = yvex_server_create(&server, &options, &err);
    if (rc != YVEX_OK) {
        return print_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 5);
    }

    if (yvex_server_get_summary(server, &summary, &err) == YVEX_OK) {
        fprintf(stderr, "yvexd listening on %s:%u\n", summary.host, summary.port);
        fprintf(stderr, "generation_available: false\n");
    }

    rc = yvex_server_serve(server, &err);
    yvex_server_close(server);
    if (rc != YVEX_OK) {
        return print_error(&err, 1);
    }
    return 0;
}
