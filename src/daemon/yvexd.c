/* Owner: daemon.yvexd (daemon).
 * Owns: daemon argument admission and the process-level server lifecycle.
 * Does not own: HTTP routing, model admission, backend policy, or generation.
 * Invariants: one invocation creates at most one server and closes it before exit.
 * Boundary: entrypoint orchestration; server APIs retain domain authority.
 * Purpose: parse daemon options and run the admitted local server shell.
 * Inputs: process arguments and operating-system resources used by the server.
 * Effects: writes operator output and may bind one listening socket.
 * Failure: rejects malformed options and returns nonzero after typed server failures. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/core.h>
#include <yvex/registry.h>
#include <yvex/server.h>

/* Purpose: render the bounded daemon command-line contract to the selected stream. */
static void print_help(FILE *fp)
{
    fprintf(fp,
            "usage: yvexd [--host HOST] [--port PORT] [--model FILE_OR_ALIAS] "
            "[--backend cpu|cuda] [--one-request]\n");
    fprintf(fp, "\n");
    fprintf(fp, "Starts the local server shell. Endpoints: /health, /metrics, /v1/models.\n");
    fprintf(fp, "--model accepts an existing GGUF path or a registered local alias.\n");
    fprintf(fp, "Generation endpoints are not implemented in server shell.\n");
}

/* Purpose: parse one decimal TCP port without truncation.
 * Inputs: immutable text and required output storage.
 * Effects: writes the validated port only on success.
 * Failure: returns false for syntax, range, or trailing-byte errors.
 * Boundary: entrypoint parsing; it does not open a socket. */
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

/* Purpose: render one typed daemon failure and preserve the requested process status. */
static int print_error(const yvex_error *err, int exit_code)
{
    fprintf(stderr, "yvexd: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

/* Purpose: own daemon execution from argument parsing through deterministic shutdown.
 * Inputs: conventional process argument count and vector.
 * Effects: may print diagnostics and create, serve, stop, and close one server.
 * Failure: returns nonzero for argument, allocation, bind, load, or serve failures.
 * Boundary: process owner; it never promotes server or generation capability. */
int main(int arg_count, char **args)
{
    yvex_server *server = NULL;
    yvex_server_options options;
    yvex_server_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    int i;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&model_ref, 0, sizeof(model_ref));
    options.host = "127.0.0.1";
    options.port = 8080;
    options.backend_name = "cpu";

    yvex_error_clear(&err);

    for (i = 1; i < arg_count; ++i) {
        if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            print_help(stdout);
            return 0;
        } else if (strcmp(args[i], "--version") == 0) {
            fprintf(stdout, "%s\n", yvex_version_string());
            return 0;
        } else if (strcmp(args[i], "--host") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "yvexd: --host requires a value\n");
                return 2;
            }
            options.host = args[++i];
        } else if (strcmp(args[i], "--port") == 0) {
            if (i + 1 >= arg_count || !parse_port(args[i + 1], &options.port)) {
                fprintf(stderr, "yvexd: --port must be in range 1..65535\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "yvexd: --model requires a value\n");
                return 2;
            }
            options.model_path = args[++i];
            options.load_engine = 1;
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "yvexd: --backend requires a value\n");
                return 2;
            }
            options.backend_name = args[++i];
        } else if (strcmp(args[i], "--one-request") == 0) {
            options.one_request = 1;
        } else {
            fprintf(stderr, "yvexd: unknown option: %s\n", args[i]);
            return 2;
        }
    }

    if (options.model_path) {
        rc = yvex_model_ref_resolve(&model_ref, options.model_path, NULL, &err);
        if (rc != YVEX_OK) {
            return print_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 5);
        }
        options.model_path = model_ref.path;
    }

    rc = yvex_server_create(&server, &options, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 5);
    }

    if (yvex_server_get_summary(server, &summary, &err) == YVEX_OK) {
        fprintf(stderr, "yvexd listening on %s:%u\n", summary.host, summary.port);
        fprintf(stderr, "generation_available: false\n");
    }

    rc = yvex_server_serve(server, &err);
    yvex_server_close(server);
    yvex_model_ref_clear(&model_ref);
    if (rc != YVEX_OK) {
        return print_error(&err, 1);
    }
    return 0;
}
