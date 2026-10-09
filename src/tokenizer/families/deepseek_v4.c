/* Exact checkpoint-owned conversation recipes; no runtime family selection. */
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/source_catalog.h>
#include <string.h>
static const char deepseek_v4_reasoning_max[] =
    "Reasoning Effort: Absolute maximum with no shortcuts permitted.\n"
    "You MUST be very thorough in your thinking and comprehensively decompose the problem to "
    "resolve the root cause, rigorously stress-testing your logic against all potential paths, "
    "edge cases, and adversarial scenarios.\n"
    "Explicitly write out your entire deliberation process, documenting every intermediate step, "
    "considered alternative, and rejected hypothesis to ensure absolutely no assumption is left "
    "unchecked.\n\n";
static const char deepseek_v4_tools_prefix[] =
    "## Tools\n\n"
    "You have access to a set of tools to help answer the user's question. You can invoke tools "
    "by writing a \"<｜DSML｜tool_calls>\" block like the following:\n\n"
    "<｜DSML｜tool_calls>\n"
    "<｜DSML｜invoke name=\"$TOOL_NAME\">\n"
    "<｜DSML｜parameter name=\"$PARAMETER_NAME\" string=\"true|false\">"
    "$PARAMETER_VALUE</｜DSML｜parameter>\n"
    "...\n"
    "</｜DSML｜invoke>\n"
    "<｜DSML｜invoke name=\"$TOOL_NAME2\">\n"
    "...\n"
    "</｜DSML｜invoke>\n"
    "</｜DSML｜tool_calls>\n\n"
    "String parameters should be specified as is and set `string=\"true\"`. For all other types "
    "(numbers, booleans, arrays, objects), pass the value in JSON format and set "
    "`string=\"false\"`.\n\n"
    "If thinking_mode is enabled (triggered by <think>), you MUST output your complete reasoning "
    "inside <think>...</think> BEFORE any tool calls or final response.\n\n"
    "Otherwise, output directly after </think> with tool calls or final response.\n\n"
    "### Available Tool Schemas\n\n";
static const char deepseek_v4_tools_suffix[] =
    "\n\nYou MUST strictly follow the above defined tool name and parameter schemas to invoke "
    "tool calls.\n";
static const char deepseek_v4_response_format[] =
    "## Response Format:\n\nYou MUST strictly adhere to the following schema to reply:\n";
static const yvex_conversation_protocol deepseek_v4_conversation = {
    .schema_version = YVEX_CONVERSATION_PROTOCOL_SCHEMA_V1,
    .family_adapter_id = YVEX_DEEPSEEK_V4_ADAPTER_ID,
    .family_adapter_version = YVEX_DEEPSEEK_V4_ADAPTER_VERSION,
    .architecture = "deepseek4",
    .source_revision = YVEX_SOURCE_RELEASE_REVISION,
    .source_encoding_path = "encoding/encoding_dsv4.py",
    .source_encoding_identity =
        "bdbd57c132a1b3725042323d02b98b9d1df28e5f388f134399555d041f5055e0",
    .bos = "<｜begin▁of▁sentence｜>",
    .eos = "<｜end▁of▁sentence｜>",
    .user = "<｜User｜>", .assistant = "<｜Assistant｜>",
    .latest_reminder = "<｜latest_reminder｜>",
    .thinking_start = "<think>", .thinking_end = "</think>",
    .tool_result_start = "<tool_result>", .tool_result_end = "</tool_result>",
    .dsml = "｜DSML｜",
    .tool_calls_start = "\n\n<｜DSML｜tool_calls>\n",
    .tool_calls_end = "</｜DSML｜tool_calls>",
    .tool_invoke_start = "<｜DSML｜invoke name=\"",
    .tool_invoke_name_end = "\">\n",
    .tool_invoke_end = "</｜DSML｜invoke>",
    .tool_parameter_start = "<｜DSML｜parameter name=\"",
    .tool_parameter_name_end = "\" string=\"",
    .tool_parameter_kind_end = "\">",
    .tool_parameter_end = "</｜DSML｜parameter>\n",
    .reasoning_effort_max = deepseek_v4_reasoning_max,
    .tools_prefix = deepseek_v4_tools_prefix, .tools_suffix = deepseek_v4_tools_suffix,
    .response_format_prefix = deepseek_v4_response_format,
    .drop_prior_reasoning_by_default = 1, .tools_preserve_reasoning = 1,
    .tool_results_merge_into_user = 1,
    .tokenizer_model = "gpt2", .tokenizer_pre = "deepseek-v3",
    .tokenizer_json_identity = "8f9f37ca37fdc4f5fd36d5cf4d3b0e8392edb4e894fd10cc0d70b4957c8633cf",
    .tokenizer_config_identity = "6ac8c8dc065ed118161d02dd532749ae3f52c243deac27872134fae2f50d8547",
    .vocabulary_size = 129280ull, .base_vocabulary_size = 128000ull,
    .merge_count = 127741ull, .added_token_count = 1283ull, .special_token_count = 1230ull,
    .bos_token_id = 0u, .eos_token_id = 1u, .pad_token_id = 1u,
    .bos_present = 1, .eos_present = 1, .pad_present = 1};


static const char reasoning_0731_max[] =
    "Reasoning Effort: Beyond maximum — exhaustive, relentless, and uncompromising.\n"
    "You MUST reason with the utmost depth and rigor, leaving absolutely nothing to chance: "
    "exhaustively decompose the problem into its most fundamental components, trace every causal "
    "chain to its root, and resolve the underlying cause rather than any surface symptom.\n"
    "Do not stop reasoning until you have independently verified the solution from multiple "
    "angles and are certain that no assumption remains unchecked and no error remains "
    "undiscovered.\n\n";

int yvex_tokenizer_deepseek_v4_conversation(
    yvex_conversation_protocol *out, const char *target_id)
{
    const yvex_source_target_identity *source;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!target_id) return 0;
    source = yvex_source_target_identity_find(target_id);
    if (!source) return 0;
    if (!strcmp(target_id, YVEX_SOURCE_RELEASE_TARGET_ID)) {
        *out = deepseek_v4_conversation;
        return 1;
    }
    if (strcmp(target_id, "deepseek4-v4-flash-0731")) return 0;
    *out = deepseek_v4_conversation;
    out->schema_version = YVEX_CONVERSATION_PROTOCOL_SCHEMA_V3;
    out->source_revision = source->upstream_revision;
    out->source_encoding_identity =
        "abc0d26120250dda0ae077dc64aa28836026e61e970854aaeb792445e6a0dde6";
    out->reasoning_effort_high = deepseek_v4_reasoning_max;
    out->reasoning_effort_max = reasoning_0731_max;
    out->reasoning_effort_low = "";
    out->system = "";
    out->message_end = out->eos;
    out->thinking_start_suffix = "";
    out->thinking_end_prefix = "";
    out->thinking_end_suffix = "";
    out->tool_result_group_start = out->user;
    return 1;
}
