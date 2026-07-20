/* Owner: src/cli/commands
 * Owns: graph command dispatch from typed input parser to report builder and renderer.
 * Does not own: graph construction, memory planning, backend probing, primitive execution, guard facts, report
 *   construction, rendering internals, generation, eval, benchmark, or release decisions.
 * Invariants: adapter stays thin: parse input, call one graph report API, render a typed report, and return an exit
 *   code.
 * Boundary: command dispatch is not graph runtime support.
 * Purpose: provide graph command dispatch from typed input parser to report builder and renderer.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/io/private.h"
#include "src/cli/model_artifacts/private.h"
#include "src/cli/render/private.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/source.h>

static volatile sig_atomic_t graph_attention_signal_seen;

/* Purpose: Record SIGINT/SIGTERM in one signal-safe scalar. */
static void graph_attention_signal_handler(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM)
        graph_attention_signal_seen = signal_number;
}

/* Purpose: Report whether the signal-safe cancellation scalar is set. */
static int graph_attention_cancel_requested(void *context) {
    (void)context;
    return graph_attention_signal_seen != 0;
}

/* Purpose: Install cancellation handlers.
 * Inputs: prior-action storage.
 * Effects: changes signal actions.
 * Failure: rolls back partial install.
 * Boundary: cancellation request only; no runtime cleanup. */
static int graph_attention_signals_install(struct sigaction *old_interrupt,
                                           struct sigaction *old_terminate, yvex_error *err) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = graph_attention_signal_handler;
    sigemptyset(&action.sa_mask);
    graph_attention_signal_seen = 0;
    if (sigaction(SIGINT, &action, old_interrupt) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "graph_attention_cli",
                        "cannot install SIGINT handler: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    if (sigaction(SIGTERM, &action, old_terminate) != 0) {
        int saved_errno = errno;
        (void)sigaction(SIGINT, old_interrupt, NULL);
        yvex_error_setf(err, YVEX_ERR_IO, "graph_attention_cli",
                        "cannot install SIGTERM handler: %s", strerror(saved_errno));
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: Restore signal actions. Inputs: saved actions. Effects: changes handlers. Failure: I/O refusal.
 * Boundary: signal lifecycle only; graph resources remain runtime-owned. */
static int graph_attention_signals_restore(const struct sigaction *old_interrupt,
                                           const struct sigaction *old_terminate, yvex_error *err) {
    int interrupt_rc = sigaction(SIGINT, old_interrupt, NULL);
    int terminate_rc = sigaction(SIGTERM, old_terminate, NULL);

    if (interrupt_rc == 0 && terminate_rc == 0)
        return YVEX_OK;
    yvex_error_set(err, YVEX_ERR_IO, "graph_attention_cli",
                   "cannot restore attention cancellation handlers");
    return YVEX_ERR_IO;
}

/* Purpose: Print one parser refusal.
 * Inputs: typed error.
 * Effects: writes CLI stderr.
 * Failure: stream state.
 * Boundary: CLI diagnostics only. */
static int graph_cli_print_parse_error(const yvex_error *err) {
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n", yvex_error_message(err));
    return 2;
}

/* Purpose: Render graph print runtime error from typed facts (`graph_cli_print_runtime_error`). */
static int graph_cli_print_runtime_error(const yvex_error *err, int exit_code) {
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n", yvex_error_where(err),
                        yvex_error_message(err));
    return exit_code;
}

/* Purpose: Build a graph report.
 * Inputs: request and output.
 * Effects: delegates domain build.
 * Failure: typed domain refusal.
 * Boundary: chooses the existing graph-report owner only. */
static int graph_cli_build_report(const yvex_graph_report_request *request,
                                  yvex_graph_report *report, yvex_error *err) {
    return request->action == YVEX_GRAPH_ACTION_DUMP
               ? yvex_graph_report_build(request, report, err)
               : yvex_graph_primitive_report_build(request, report, err);
}

typedef struct {
    yvex_graph_attention_operator_request request;
    yvex_operator_paths operator_paths;
    char source_path[YVEX_PATH_CAP];
    char source_manifest_path[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
} graph_attention_request;

/* Purpose: Resolve attention paths.
 * Inputs: CLI args and output.
 * Effects: builds a request.
 * Failure: typed path refusal.
 * Boundary: no source, artifact, or runtime open. */
static int graph_cli_attention_request_build(const yvex_graph_args *args,
                                             graph_attention_request *out, yvex_error *err) {
    yvex_paths paths = {0};
    const yvex_source_target_identity *release = yvex_source_release_identity();
    char gguf_dir[YVEX_PATH_CAP];
    int exists = 0;
    int rc;

    if (!args || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph_attention_cli",
                       "attention arguments and request output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!release || !release->family_key) {
        yvex_error_set(err, YVEX_ERR_STATE, "graph_attention_cli",
                       "release target identity is unavailable");
        return YVEX_ERR_STATE;
    }
    out->request.target = args->attention.target;
    out->request.source_path = out->source_path;
    out->request.models_root = out->operator_paths.models_root;
    out->request.source_manifest_path = out->source_manifest_path;
    out->request.artifact_path = out->artifact_path;
    out->request.probe = YVEX_ATTENTION_PROBE_CANONICAL;
    out->request.scope = strcmp(args->attention.scope, "full") == 0
                             ? YVEX_ATTENTION_PROBE_SCOPE_FULL
                             : YVEX_ATTENTION_PROBE_SCOPE_QUICK;
    out->request.compare_backends = args->attention.compare_backends;
    out->request.backend = YVEX_BACKEND_KIND_CPU;
    if (!yvex_source_is_release_target(args->attention.target))
        return YVEX_OK;
    rc = yvex_operator_paths_resolve(&paths, args->attention.models_root,
                                     &out->operator_paths, err);
    if (rc != YVEX_OK)
        return rc;
    rc = yvex_operator_paths_resolve_target(&out->operator_paths, release->family_key, "source",
                                            out->source_path, sizeof(out->source_path), &exists,
                                            err);
    if (rc != YVEX_OK)
        return rc;
    rc = yvex_operator_paths_resolve_target(&out->operator_paths, release->family_key, "gguf", gguf_dir,
                                            sizeof(gguf_dir), &exists, err);
    if (rc != YVEX_OK)
        return rc;
    rc = path_join2(out->source_manifest_path, sizeof(out->source_manifest_path), gguf_dir,
                    "deepseek-source-manifest.json", err, "graph_attention_cli");
    if (rc != YVEX_OK)
        return rc;
    if (args->attention.artifact_path) {
        rc = expand_operator_path(args->attention.artifact_path, out->artifact_path,
                                  sizeof(out->artifact_path), err, "graph_attention_cli");
    } else {
        rc = path_join2(out->artifact_path, sizeof(out->artifact_path), gguf_dir,
                        YVEX_SELECTED_DEEPSEEK_ARTIFACT_FILENAME, err, "graph_attention_cli");
    }
    if (rc != YVEX_OK)
        return rc;

    out->request.models_root = out->operator_paths.models_root;
    if (args->attention.compare_backends) {
        out->request.backend = YVEX_BACKEND_KIND_CPU;
        return YVEX_OK;
    }
    return yvex_backend_kind_parse(args->attention.backend, &out->request.backend, err);
}

/* Purpose: Execute attention.
 * Inputs: CLI args and runtime API.
 * Effects: renders a typed result.
 * Failure: nonzero CLI status.
 * Boundary: no attention math, oracle, or test indirection. */
static int graph_cli_attention_execute(const yvex_graph_args *args, yvex_error *err) {
    graph_attention_request request;
    yvex_graph_attention_operator_result result;
    struct sigaction old_interrupt;
    struct sigaction old_terminate;
    yvex_error restore_error;
    int render_rc;
    int restore_rc;
    int cancellation_seen;
    int rc;

    memset(&request, 0, sizeof(request));
    memset(&result, 0, sizeof(result));
    rc = graph_cli_attention_request_build(args, &request, err);
    if (rc != YVEX_OK)
        return graph_cli_print_runtime_error(err, exit_for_status(rc));
    rc = graph_attention_signals_install(&old_interrupt, &old_terminate, err);
    if (rc != YVEX_OK)
        return graph_cli_print_runtime_error(err, exit_for_status(rc));
    request.request.cancel_requested = graph_attention_cancel_requested;
    rc = yvex_graph_attention_operator_execute(&request.request, &result, err);
    yvex_error_clear(&restore_error);
    restore_rc = graph_attention_signals_restore(&old_interrupt, &old_terminate, &restore_error);
    cancellation_seen = graph_attention_signal_seen != 0;
    graph_attention_signal_seen = 0;
    if (restore_rc != YVEX_OK) {
        *err = restore_error;
        result.completed = 0;
        (void)snprintf(result.status, sizeof(result.status), "failed");
        (void)snprintf(result.reason, sizeof(result.reason), "%s",
                       yvex_error_message(&restore_error));
        rc = restore_rc;
    } else if (cancellation_seen && rc == YVEX_OK) {
        yvex_error_set(err, YVEX_ERR_CANCELLED, "graph_attention_cli",
                       "attention execution cancelled before command publication");
        result.completed = 0;
        (void)snprintf(result.status, sizeof(result.status), "cancelled");
        (void)snprintf(result.reason, sizeof(result.reason), "%s", yvex_error_message(err));
        rc = YVEX_ERR_CANCELLED;
    }
    if (result.status[0]) {
        FILE *output = yvex_cli_out_stdout();

        render_rc = yvex_graph_attention_render(output, args->render_mode, &result);
        if (render_rc == YVEX_OK)
            render_rc = yvex_cli_out_flush(output);
        if (render_rc != YVEX_OK) {
            yvex_error_set(err, render_rc, "graph_attention_cli",
                           "attention result rendering failed");
            return graph_cli_print_runtime_error(err, exit_for_status(render_rc));
        }
    }
    if (rc != YVEX_OK)
        return graph_cli_print_runtime_error(err, exit_for_status(rc));
    if (!result.completed) {
        yvex_error_set(err, YVEX_ERR_STATE, "graph_attention_cli",
                       "attention execution returned an incomplete result");
        return graph_cli_print_runtime_error(err, exit_for_status(YVEX_ERR_STATE));
    }
    return 0;
}

/* Purpose: Dispatch graph.
 * Inputs: argv.
 * Effects: executes and renders a typed request.
 * Failure: nonzero CLI status.
 * Boundary: domain owners retain capability truth. */
int yvex_graph_command(int argc, char **argv) {
    yvex_graph_args args;
    yvex_graph_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&report, 0, sizeof(report));

    rc = yvex_graph_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return graph_cli_print_parse_error(&err);
    }

    if (args.help_requested) {
        (void)yvex_graph_render_help(yvex_cli_out_stdout());
        return args.help_exit_code;
    }

    if (args.attention.execute) {
        return graph_cli_attention_execute(&args, &err);
    }

    rc = graph_cli_build_report(&args.request, &report, &err);
    if (rc != YVEX_OK) {
        int exit_code = report.exit_code ? report.exit_code : exit_for_status(rc);
        if (report.body) {
            (void)yvex_graph_render(yvex_cli_out_stdout(), args.render_mode, &report);
        }
        yvex_graph_report_clear(&report);
        return graph_cli_print_runtime_error(&err, exit_code);
    }

    (void)yvex_graph_render(yvex_cli_out_stdout(), args.render_mode, &report);
    rc = report.exit_code;
    yvex_graph_report_clear(&report);
    return rc;
}

/* Purpose: Render graph help.
 * Inputs: output stream.
 * Effects: writes CLI text.
 * Failure: stream state.
 * Boundary: CLI presentation. */
void yvex_graph_help(FILE *fp) {
    (void)yvex_graph_render_help(fp);
}
