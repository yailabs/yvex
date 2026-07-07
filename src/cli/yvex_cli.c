/*
 * yvex_cli.c - top-level operator CLI dispatch.
 *
 * Owner:
 *   src/cli
 *
 * Owns:
 *   top-level command lookup, short help, grouped command catalog, command
 *   metadata, and argv dispatch.
 *
 * Does not own:
 *   domain behavior, long command help, model/artifact/runtime logic, report
 *   building, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   top-level metadata remains short and dispatch-only; detailed usage and
 *   behavior stay in the owner module for each command; unknown commands return
 *   parser-style failures.
 *
 * Boundary:
 *   top-level CLI dispatch cannot imply lower-level capability, model support,
 *   generation, eval, benchmark, throughput, or release readiness.
 */

#include <stdio.h>
#include <string.h>
#include "yvex_cli_out.h"

#include "yvex_operator_private.h"
#include "yvex_render_private.h"
#include "yvex/version.h"

typedef int (*yvex_cli_handler_fn)(int argc, char **argv);
typedef void (*yvex_cli_help_fn)(FILE *fp);

typedef struct {
    const char *name;
    const char *group;
    const char *surface;
    const char *purpose;
    const char *usage;
    const char *example;
    const char *option_classes;
    const char *boundary;
    yvex_cli_handler_fn handler;
    yvex_cli_help_fn help;
} yvex_cli_command;

static int command_commands(int argc, char **argv);
static int command_help(int argc, char **argv);
static int command_version(int argc, char **argv);
static void command_commands_help(FILE *fp);
static void command_help_help(FILE *fp);
static void command_version_help(FILE *fp);

#define CMD(N,G,S,P,U,E,O,B,H,HF) { N, G, S, P, U, E, O, B, H, HF }

static const yvex_cli_command yvex_commands[] = {
    CMD("accounts","operator","mixed-transitional","Provider account status.","yvex accounts status [provider]","yvex accounts status","selector, diagnostic, transitional-layout","local provider observation only",yvex_accounts_command,yvex_accounts_help),
    CMD("attention","model","diagnostic","Attention requirement reports.","yvex attention report --model TARGET","yvex attention report --model TARGET","selector, path, diagnostic, transitional-layout","report-only; not full attention runtime",yvex_attention_command,yvex_attention_help),
    CMD("backend","diagnostic","diagnostic","Backend availability reports.","yvex backend cpu|cuda","yvex backend cuda","selector","backend status is not model support",yvex_backend_command,yvex_backend_help),
    CMD("chat","runtime","diagnostic","Accepted-only diagnostic console.","yvex chat [--model TARGET]","yvex chat --backend cpu","selector, path, behavior, diagnostic","diagnostic console; no generation",yvex_chat_command,yvex_chat_help),
    CMD("commands","core","porcelain","Grouped command catalog.","yvex commands","yvex commands","none","catalog only; no domain execution",command_commands,command_commands_help),
    CMD("context","runtime","diagnostic","Context class and boundary reports.","yvex context report --model TARGET","yvex context report --model TARGET","selector, path, diagnostic, transitional-layout","report-only; no long-context runtime",yvex_context_command,yvex_context_help),
    CMD("convert","source","mixed-transitional","Selected conversion planning.","yvex convert plan|emit [options]","yvex convert plan --arch qwen","selector, path, behavior, diagnostic","conversion tooling only; no runtime claim",yvex_convert_command,yvex_convert_help),
    CMD("cuda-info","diagnostic","diagnostic","CUDA device facts.","yvex cuda-info","yvex cuda-info","none","device probe only; not CUDA model runtime",yvex_cuda_info_command,yvex_cuda_info_help),
    CMD("decode","runtime","diagnostic","Bounded diagnostic decode step.","yvex decode --model TARGET --backend cpu|cuda","yvex decode --model TARGET --backend cpu --tokens 0,1","selector, behavior, diagnostic","diagnostic decode only; no generation",yvex_decode_command,yvex_decode_help),
    CMD("detokenize","diagnostic","diagnostic","Decode token IDs.","yvex detokenize FILE --ids IDS","yvex detokenize FILE --ids 1,2","selector","tokenizer diagnostic only",yvex_detokenize_command,yvex_detokenize_help),
    CMD("engine","runtime","diagnostic","Open an engine descriptor.","yvex engine [--model] TARGET [--backend cpu|cuda]","yvex engine --model TARGET --backend cpu","selector, path, diagnostic","engine descriptor only; no decode or generation",yvex_engine_command,yvex_engine_help),
    CMD("fullmodel","model","mixed-transitional","Fullmodel inventory and plan reports.","yvex fullmodel report --model TARGET","yvex fullmodel report --model TARGET","selector, path, behavior, diagnostic, transitional-layout","report/materialization planning only unless subcommand proves more",yvex_fullmodel_command,yvex_fullmodel_help),
    CMD("graph","graph","mixed-transitional","Graph diagnostics and narrow proofs.","yvex graph [--model] TARGET","yvex graph check --suite primitives --backend cpu","selector, behavior, diagnostic","graph proof is not generation",yvex_graph_command,yvex_graph_help),
    CMD("generate","runtime","diagnostic","Bounded diagnostic generation loop.","yvex generate --model TARGET --backend cpu|cuda","yvex generate --model TARGET --backend cpu --tokens IDS --max-new-tokens N","selector, behavior, diagnostic","diagnostic generation only; full model generation unsupported",yvex_generate_command,yvex_generate_help),
    CMD("gguf-template","artifact","diagnostic","Template validation.","yvex gguf-template validate FILE","yvex gguf-template inspect FILE","path, diagnostic","template validation only",yvex_gguf_template_command,yvex_gguf_template_help),
    CMD("gguf-emit","artifact","mixed-transitional","Controlled artifact emission.","yvex gguf-emit [options]","yvex gguf-emit --help","path, behavior, diagnostic","controlled artifact tooling; no inference",yvex_gguf_emit_command,yvex_gguf_emit_help),
    CMD("help","core","porcelain","Command help.","yvex help [command]","yvex help models","none","help only; no domain execution",command_help,command_help_help),
    CMD("imatrix","source","diagnostic","Imatrix manifest diagnostics.","yvex imatrix create|inspect|validate [options]","yvex imatrix validate FILE","path, behavior, diagnostic","manifest tooling only",yvex_imatrix_command,yvex_imatrix_help),
    CMD("info","core","porcelain","Build and boundary status.","yvex info [--audit | --output normal|audit]","yvex info","diagnostic, transitional-layout","status summary only",yvex_runtime_info_command,yvex_runtime_info_help),
    CMD("inspect","artifact","diagnostic","Artifact descriptor inspection.","yvex inspect FILE","yvex inspect FILE","path, diagnostic","descriptor only; no materialization",yvex_inspect_command,yvex_inspect_help),
    CMD("input","runtime","diagnostic","Token input validation.","yvex input tokens --model TARGET --tokens IDS","yvex input tokens --model TARGET --tokens 0,1","selector, behavior, diagnostic","input validation only",yvex_input_command,yvex_input_help),
    CMD("integrity","artifact","mixed-transitional","Artifact integrity reports.","yvex integrity check|report --model TARGET","yvex integrity report --model TARGET --backend cpu","selector, path, diagnostic, transitional-layout","integrity evidence is not execution",yvex_integrity_command,yvex_integrity_help),
    CMD("kv","runtime","mixed-transitional","KV diagnostics and reports.","yvex kv report --model TARGET","yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8","selector, behavior, diagnostic, transitional-layout","diagnostic/report-only KV unless subcommand proves more",yvex_kv_command,yvex_kv_help),
    CMD("logits","runtime","diagnostic","Bounded diagnostic logits buffer.","yvex logits --model TARGET --backend cpu|cuda","yvex logits --model TARGET --backend cpu --tokens 0,1","selector, behavior, diagnostic","diagnostic logits only; no output-head runtime",yvex_logits_command,yvex_logits_help),
    CMD("materialize","artifact","mixed-transitional","Materialize selected weights.","yvex materialize --model TARGET --backend cpu|cuda","yvex materialize --model TARGET --backend cpu","selector, path, behavior, diagnostic","materialization is not execution",yvex_materialize_command,yvex_materialize_help),
    CMD("materialize-gate","artifact","diagnostic","Materialization hardening gate.","yvex materialize-gate check --model TARGET","yvex materialize-gate check --model TARGET --label LABEL","selector, path, behavior, diagnostic","gate evidence only",yvex_materialize_gate_command,yvex_materialize_gate_help),
    CMD("metadata","artifact","diagnostic","Parsed metadata entries.","yvex metadata FILE","yvex metadata FILE","path, diagnostic","metadata inspection only",yvex_metadata_command,yvex_metadata_help),
    CMD("model-gate","artifact","diagnostic","Selected artifact gate.","yvex model-gate check --model TARGET","yvex model-gate check --model TARGET --label LABEL","selector, path, behavior, diagnostic","gate evidence only",yvex_model_gate_command,yvex_model_gate_help),
    CMD("model-target","model","mixed-transitional","Model pressure target reports.","yvex model-target <action> [TARGET]","yvex model-target inspect qwen3-8b","selector, path, diagnostic, transitional-layout","target reports are not capability claims",yvex_model_target_command,yvex_model_target_help),
    CMD("moe","model","diagnostic","MoE model-class reports.","yvex moe report --model TARGET","yvex moe report --model TARGET","selector, path, diagnostic, transitional-layout","report-only; no MoE runtime",yvex_moe_command,yvex_moe_help),
    CMD("models","operator","mixed-transitional","Local model registry and source lanes.","yvex models <action> [TARGET]","yvex models list","selector, path, behavior, diagnostic, transitional-layout","operator registry/source UX; no runtime claim",yvex_models_command,yvex_models_help),
    CMD("native-weights","source","diagnostic","Safetensors header inventory.","yvex native-weights --source DIR","yvex native-weights --source DIR --limit 20","path, behavior, diagnostic","header-only inventory; no payload loading",yvex_native_weights_command,yvex_native_weights_help),
    CMD("paths","operator","porcelain","Operator filesystem paths.","yvex paths [--create]","yvex paths","path, behavior, diagnostic, transitional-layout","path reporting only",yvex_paths_command,yvex_paths_help),
    CMD("plan","graph","diagnostic","Estimate-only execution plan.","yvex plan FILE [--backend cpu|cuda]","yvex plan FILE","selector, path, diagnostic","plan only; no execution",yvex_plan_command,yvex_plan_help),
    CMD("prefill","runtime","diagnostic","Diagnostic prefill state summary.","yvex prefill --model TARGET --backend cpu|cuda","yvex prefill --model TARGET --backend cpu --tokens 0,1","selector, behavior, diagnostic","diagnostic prefill only; no full transformer prefill",yvex_prefill_command,yvex_prefill_help),
    CMD("prompt","diagnostic","diagnostic","Bounded prompt rendering.","yvex prompt FILE --user TEXT","yvex prompt FILE --user hello","path, behavior, diagnostic","prompt diagnostic only",yvex_prompt_command,yvex_prompt_help),
    CMD("quant-job","source","diagnostic","Quantization job manifest.","yvex quant-job create|inspect|validate [options]","yvex quant-job validate FILE","path, behavior, diagnostic","job manifest only; no quantization",yvex_quant_job_command,yvex_quant_job_help),
    CMD("quant-policy","source","diagnostic","Quantization policy manifest.","yvex quant-policy inspect|validate|derive [options]","yvex quant-policy validate FILE","path, behavior, diagnostic","policy/report only; no quantization",yvex_quant_policy_command,yvex_quant_policy_help),
    CMD("qtype-support","source","diagnostic","Qtype support policy.","yvex qtype-support","yvex qtype-support","diagnostic","support policy only; no per-role qtype completion",yvex_qtype_support_command,yvex_qtype_support_help),
    CMD("run","runtime","diagnostic","Accepted prompt diagnostics.","yvex run --model FILE --backend cpu|cuda --prompt TEXT","yvex run --model TARGET --backend cpu --prompt hello","selector, path, behavior, diagnostic","accepted-only runtime shell; no generation",yvex_run_command,yvex_run_help),
    CMD("sample","runtime","diagnostic","Bounded diagnostic token selection.","yvex sample --model TARGET --backend cpu|cuda","yvex sample --model TARGET --backend cpu --tokens 0,1","selector, behavior, diagnostic","diagnostic sampling only",yvex_sample_command,yvex_sample_help),
    CMD("session","runtime","diagnostic","Engine/session diagnostic state.","yvex session TARGET [--backend cpu|cuda]","yvex session TARGET --backend cpu","selector, path, behavior, diagnostic","diagnostic session only; no generation",yvex_session_command,yvex_session_help),
    CMD("source-manifest","source","mixed-transitional","Source provenance reports.","yvex source-manifest create|report [options]","yvex source-manifest report --family qwen --release v0.1.0","selector, path, behavior, diagnostic, transitional-layout","source evidence only; no artifact/runtime",yvex_source_manifest_command,yvex_source_manifest_help),
    CMD("tensor-collection","model","diagnostic","Tensor collection reports.","yvex tensor-collection report --model TARGET","yvex tensor-collection report --model TARGET --collection moe","selector, path, diagnostic, transitional-layout","report-only; no graph consumer",yvex_tensor_collection_command,yvex_tensor_collection_help),
    CMD("tensor-map","source","diagnostic","Native tensor role mapping.","yvex tensor-map [options]","yvex tensor-map --arch qwen","selector, path, behavior, diagnostic","mapping report/tooling only; no runtime descriptor",yvex_tensor_map_command,yvex_tensor_map_help),
    CMD("tokenize","diagnostic","diagnostic","Encode text with tokenizer.","yvex tokenize FILE --text TEXT","yvex tokenize FILE --text hello","path, behavior","tokenizer diagnostic only",yvex_tokenize_command,yvex_tokenize_help),
    CMD("tokenizer","diagnostic","diagnostic","Tokenizer metadata inspection.","yvex tokenizer FILE","yvex tokenizer FILE","path, diagnostic","metadata inspection only",yvex_tokenizer_command,yvex_tokenizer_help),
    CMD("tensors","artifact","diagnostic","Tensor table rows.","yvex tensors FILE","yvex tensors FILE","path, diagnostic","tensor directory inspection only",yvex_tensors_command,yvex_tensors_help),
    CMD("version","core","porcelain","Print the YVEX version.","yvex version","yvex version","none","version only",command_version,command_version_help),
};

#undef CMD

static const unsigned long yvex_command_count = sizeof(yvex_commands) / sizeof(yvex_commands[0]);
static const char *yvex_command_groups[] = {
    "core", "operator", "model", "source", "artifact",
    "graph", "runtime", "diagnostic", "server", "research/future"
};
static const unsigned long yvex_command_group_count =
    sizeof(yvex_command_groups) / sizeof(yvex_command_groups[0]);

static const yvex_cli_command *find_command(const char *name)
{
    unsigned long i;

    if (!name) return NULL;
    for (i = 0; i < yvex_command_count; ++i) {
        if (strcmp(yvex_commands[i].name, name) == 0) return &yvex_commands[i];
    }
    return NULL;
}

static int command_group_has_entries(const char *group)
{
    unsigned long i;

    for (i = 0; i < yvex_command_count; ++i) {
        if (strcmp(yvex_commands[i].group, group) == 0) return 1;
    }
    return 0;
}

/*
 * print_command_catalog()
 *
 * Purpose:
 *   render the grouped top-level command catalog.
 *
 * Inputs:
 *   fp is a borrowed output stream; command metadata is static.
 *
 * Effects:
 *   prints table output only and does not call domain command handlers.
 *
 * Failure:
 *   no parser failure path; stream errors are left to stdio.
 *
 * Boundary:
 *   catalog rendering is command discovery only and cannot claim runtime,
 *   generation, eval, benchmark, or release support.
 */
static void print_command_catalog(FILE *fp)
{
    yvex_render_out out;
    unsigned long g;
    unsigned long i;

    yvex_render_out_init(&out, fp, YVEX_RENDER_MODE_TABLE);
    yvex_render_section(&out, "YVEX COMMAND CATALOG");
    for (g = 0; g < yvex_command_group_count; ++g) {
        const char *group = yvex_command_groups[g];
        if (!command_group_has_entries(group)) continue;
        fputc('\n', fp);
        yvex_render_kv(&out, "group", group);
        yvex_render_table_header(&out,
            "  COMMAND            GROUP            SURFACE              PURPOSE");
        for (i = 0; i < yvex_command_count; ++i) {
            char row[512];
            if (strcmp(yvex_commands[i].group, group) != 0) continue;
            snprintf(row, sizeof(row), "  %-18s %-16s %-20s %s",
                     yvex_commands[i].name, yvex_commands[i].group,
                     yvex_commands[i].surface, yvex_commands[i].purpose);
            yvex_render_table_row(&out, row);
        }
    }
}

/*
 * print_top_level_help()
 *
 * Purpose:
 *   render compact top-level help and common command shapes.
 *
 * Inputs:
 *   fp is a borrowed output stream.
 *
 * Effects:
 *   prints porcelain help text; it does not run command logic or inspect local
 *   artifacts.
 *
 * Failure:
 *   no parser failure path; stream errors are left to stdio.
 *
 * Boundary:
 *   top-level help is not evidence of lower-level feature support.
 */
static void print_top_level_help(FILE *fp)
{
    yvex_render_out out;

    yvex_render_out_init(&out, fp, YVEX_RENDER_MODE_PORCELAIN);
    yvex_render_section(&out, "yvex - local-first inference engine");
    yvex_cli_out_writef(fp, "\nusage:\n  yvex <command> [args]\n  yvex help <command>\n  yvex commands\n");
    yvex_cli_out_writef(fp, "\ncommand shape:\n  yvex <family> <action> [object] [selectors] [behavior flags] [diagnostic flags]\n");
    yvex_cli_out_writef(fp, "\ncommon:\n  yvex paths\n  yvex models list\n  yvex models prepare TARGET\n  yvex models check TARGET\n  yvex model-target inspect TARGET\n  yvex graph check --suite primitives --backend cpu\n  yvex generate --model TARGET --backend cpu --tokens IDS --max-new-tokens N\n");
    yvex_cli_out_writef(fp, "\ncommand groups:\n  core: help, commands, info, version\n  operator: paths, models, accounts\n  model/source: model-target, fullmodel, source-manifest, native-weights, tensor-map\n  artifact: inspect, metadata, tensors, integrity, materialize, gguf-template, gguf-emit\n  graph: graph, plan\n  runtime: input, engine, session, prefill, kv, decode, logits, sample, generate\n  diagnostics: backend, cuda-info, tokenizer, tokenize, detokenize, prompt, chat, run\n  server: yvexd daemon status surface\n  research/future: documented future lanes only\n");
    yvex_cli_out_writef(fp, "\noption classes:\n  selector: --model, --backend, --role, --gate\n  path: --models-root, --source, --out, --out-dir, --registry\n  behavior: --dry-run, --overwrite, --no-register, --no-use\n  diagnostic: --audit, --trace-level, --include-*\n  plumbing: --json where implemented\n  transitional layout: --output normal|table|audit where implemented\n");
    yvex_cli_out_writef(fp, "\nraw/evidence:\n  --audit shows evidence where implemented\n  --json is the target plumbing surface where implemented\n\n");
    yvex_render_boundary(&out,
        "full model generation remains unsupported unless a specific command proves otherwise");
    yvex_cli_out_writef(fp, "\nUse 'yvex commands' for the grouped command catalog.\n");
}

static int command_commands(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    print_command_catalog(stdout);
    return 0;
}

/*
 * command_help()
 *
 * Purpose:
 *   route top-level help requests to short metadata and domain-owned help.
 *
 * Inputs:
 *   argc/argv are borrowed CLI arguments.
 *
 * Effects:
 *   prints help text and may call the selected command's help function; it does
 *   not call command behavior handlers.
 *
 * Failure:
 *   returns parser failure for unknown help topics.
 *
 * Boundary:
 *   delegated help describes command grammar only and does not create runtime
 *   capability or readiness claims.
 */
static int command_help(int argc, char **argv)
{
    const yvex_cli_command *command;
    yvex_render_out out;

    if (argc <= 2) {
        print_top_level_help(stdout);
        return 0;
    }
    command = find_command(argv[2]);
    if (!command) {
        yvex_cli_out_writef(stderr, "yvex: unknown help topic: %s\n", argv[2]);
        yvex_cli_out_writef(stderr, "Try 'yvex commands' for the grouped command catalog.\n");
        return 2;
    }
    yvex_render_out_init(&out, stdout, YVEX_RENDER_MODE_PORCELAIN);
    yvex_render_report_title(&out, "command", command->name, command->surface);
    yvex_render_kv(&out, "group", command->group);
    yvex_render_kv(&out, "purpose", command->purpose);
    yvex_render_kv(&out, "usage", command->usage);
    yvex_render_kv(&out, "example", command->example);
    yvex_render_kv(&out, "option_classes", command->option_classes);
    yvex_render_boundary(&out, command->boundary);
    yvex_cli_out_writef(stdout, "\ndomain help:\n");
    command->help(stdout);
    return 0;
}

static int command_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    yvex_cli_out_writef(stdout, "yvex %s\n", yvex_version_string());
    return 0;
}

static void command_commands_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex commands\n\nPrints the grouped command catalog with command, group, surface class, and purpose.\n");
}

static void command_help_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex help [command]\n\nPrints top-level help or grammar-first help for one implemented command.\n");
}

static void command_version_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex version\n\nPrints the same version string as yvex --version.\n");
}

/*
 * main()
 *
 * Purpose:
 *   dispatch argv to the selected top-level command handler.
 *
 * Inputs:
 *   argc/argv are borrowed from the host process.
 *
 * Effects:
 *   prints top-level help/version or calls exactly one registered command
 *   handler; it does not implement domain behavior inline.
 *
 * Failure:
 *   returns parser failure for unknown commands and otherwise propagates the
 *   selected command handler result.
 *
 * Boundary:
 *   argv dispatch is not runtime execution, generation support, eval evidence,
 *   benchmark evidence, throughput, or release readiness.
 */
int main(int argc, char **argv)
{
    const yvex_cli_command *command;

    if (argc == 1) {
        print_top_level_help(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_top_level_help(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        return command_version(argc, argv);
    }
    command = find_command(argv[1]);
    if (command) {
        return command->handler(argc, argv);
    }
    yvex_cli_out_writef(stderr, "yvex: unknown command: %s\n", argv[1]);
    yvex_cli_out_writef(stderr, "Try 'yvex help' for usage.\n");
    return 2;
}
