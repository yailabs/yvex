/* Deployment capacity reflects current host and cgroup pressure, never package identity. */
#include "src/runtime/private.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yvex/internal/core.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/moe.h>

static int runtime_capacity_value(const char *text, unsigned long long *value)
{
    char *tail = NULL;
    unsigned long long parsed;
    if (!text || !value || !text[0] || text[0] == '-' || !strncmp(text, "max", 3u))
        return 0;
    errno = 0;
    parsed = strtoull(text, &tail, 10);
    if (errno || tail == text) return 0;
    while (*tail == ' ' || *tail == '\t' || *tail == '\r' || *tail == '\n') ++tail;
    if (*tail) return 0;
    *value = parsed;
    return 1;
}

/* Select the tightest remaining cgroup-v2 memory extent across the process hierarchy. */
static int runtime_cgroup_memory(unsigned long long *capacity,
                                 unsigned long long *available)
{
    const char *injected = getenv("YVEX_TEST_RUNTIME_CGROUP_AVAILABLE_MEMORY_BYTES");
    static const char *const controls[] = {"memory.max", "memory.high"};
    const char *root = "/sys/fs/cgroup";
    char group[PATH_MAX], directory[PATH_MAX], path[PATH_MAX], text[128];
    char *relative, *newline, *slash;
    unsigned long long tightest_capacity = ULLONG_MAX;
    unsigned long long tightest_available = ULLONG_MAX;
    size_t control, root_length = strlen(root);
    int found = 0, written;
    if (!capacity || !available) return -1;
    if (injected) {
        if (!runtime_capacity_value(injected, available)) return -1;
        *capacity = ULLONG_MAX;
        return 1;
    }
    if (!yvex_core_file_read_text_prefix("/proc/self/cgroup", group, sizeof(group)) ||
        !(relative = strstr(group, "0::")) ||
        (relative != group && relative[-1] != '\n') ||
        relative[3] != '/' || strstr(relative + 3, ".."))
        return 0;
    relative += 3;
    if ((newline = strchr(relative, '\n'))) *newline = '\0';
    written = snprintf(directory, sizeof(directory), "%s%s", root, relative);
    if (written <= 0 || (size_t)written >= sizeof(directory)) return -1;
    for (;;) {
        unsigned long long current;
        int current_known;
        written = snprintf(path, sizeof(path), "%s/memory.current", directory);
        if (written <= 0 || (size_t)written >= sizeof(path)) return -1;
        current_known = yvex_core_file_read_text_prefix(path, text, sizeof(text)) &&
                        runtime_capacity_value(text, &current);
        for (control = 0u; current_known && control < 2u; ++control) {
            unsigned long long limit, remaining;
            written = snprintf(path, sizeof(path), "%s/%s", directory, controls[control]);
            if (written <= 0 || (size_t)written >= sizeof(path)) return -1;
            if (!yvex_core_file_read_text_prefix(path, text, sizeof(text)) ||
                !runtime_capacity_value(text, &limit))
                continue;
            remaining = current < limit ? limit - current : 0ull;
            if (!found || limit < tightest_capacity) tightest_capacity = limit;
            if (!found || remaining < tightest_available)
                tightest_available = remaining;
            found = 1;
        }
        if (!strcmp(directory, root)) break;
        slash = strrchr(directory, '/');
        if (!slash || (size_t)(slash - directory) < root_length) return -1;
        *slash = '\0';
    }
    if (found) {
        *capacity = tightest_capacity;
        *available = tightest_available;
    }
    return found;
}

static int runtime_system_memory(unsigned long long *total,
                                 unsigned long long *available)
{
    const char *injected_total = getenv("YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES");
    const char *injected_available =
        getenv("YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES");
    char line[128];
    FILE *meminfo;
    unsigned long long system_total = 0ull, system_available = 0ull;
    long total_pages, available_pages, page_bytes;

    if (!total || !available) return 0;
    if (injected_total && !runtime_capacity_value(injected_total, &system_total)) return 0;
    if (injected_available &&
        !runtime_capacity_value(injected_available, &system_available))
        return 0;
    if ((!system_total || !system_available) &&
        (meminfo = fopen("/proc/meminfo", "r"))) {
        while (fgets(line, sizeof(line), meminfo)) {
            unsigned long long value;
            if (!system_total && sscanf(line, "MemTotal: %llu kB", &value) == 1)
                if (!yvex_core_u64_mul(value, 1024ull, &system_total))
                    system_total = 0ull;
            if (!system_available &&
                sscanf(line, "MemAvailable: %llu kB", &value) == 1)
                if (!yvex_core_u64_mul(value, 1024ull, &system_available))
                    system_available = 0ull;
        }
        if (fclose(meminfo) != 0) return 0;
    }
    if (!system_total || !system_available) {
#if defined(_SC_PHYS_PAGES) && defined(_SC_AVPHYS_PAGES)
        total_pages = sysconf(_SC_PHYS_PAGES);
        available_pages = sysconf(_SC_AVPHYS_PAGES);
        page_bytes = sysconf(_SC_PAGESIZE);
        if (total_pages <= 0 || available_pages <= 0 || page_bytes <= 0 ||
            (!system_total &&
             !yvex_core_u64_mul((unsigned long long)total_pages,
                                (unsigned long long)page_bytes, &system_total)) ||
            (!system_available &&
             !yvex_core_u64_mul((unsigned long long)available_pages,
                                (unsigned long long)page_bytes,
                                &system_available)))
            return 0;
#else
        (void)total_pages;
        (void)available_pages;
        (void)page_bytes;
        return 0;
#endif
    }
    if (system_available > system_total) system_available = system_total;
    *total = system_total;
    *available = system_available;
    return 1;
}

int yvex_runtime_private_memory_capacity(unsigned long long *total_bytes,
                                         unsigned long long *available_bytes,
                                         int *process_limited)
{
    unsigned long long total, available, process_capacity, process_available;
    int cgroup;

    if (!total_bytes || !available_bytes || !process_limited ||
        !runtime_system_memory(&total, &available))
        return 0;
    *process_limited = 0;
    cgroup = runtime_cgroup_memory(&process_capacity, &process_available);
    if (cgroup < 0) return 0;
    if (cgroup > 0) {
        int limited = process_capacity < total || process_available < available;
        if (process_capacity < total) total = process_capacity;
        if (process_available < available) available = process_available;
        *process_limited = limited;
    }
    if (available > total) available = total;
    *total_bytes = total;
    *available_bytes = available;
    return 1;
}

unsigned long long yvex_runtime_private_system_reserve(
    unsigned long long capacity_bytes)
{
    unsigned long long proportional = capacity_bytes / 8ull;
    return proportional > YVEX_EXECUTION_MINIMUM_SYSTEM_RESERVE
               ? proportional
               : YVEX_EXECUTION_MINIMUM_SYSTEM_RESERVE;
}


typedef struct {
    const yvex_model_engine_view *model_view;
    yvex_runtime_capacity_options options;
    yvex_runtime_capacity result;
} runtime_capacity_context;

static int capacity_context_refuse(yvex_error *err, yvex_status status,
                                    const char *reason)
{
    yvex_error_set(err, status, "runtime.capacity", reason);
    return status;
}

static int capacity_options_valid(const yvex_runtime_capacity_options *options)
{
    return options &&
        (options->backend == YVEX_BACKEND_KIND_CPU ||
         options->backend == YVEX_BACKEND_KIND_CUDA) &&
        options->mode <= YVEX_EXECUTION_GENERATION_SPECULATIVE &&
        options->workload_kind <= YVEX_EXECUTION_WORKLOAD_FULL_MODEL_RESEARCH &&
        options->evidence_profile <= YVEX_EXECUTION_EVIDENCE_FORENSIC &&
        options->sampling_requirement <= YVEX_EXECUTION_SAMPLING_STOCHASTIC &&
        options->context_capacity && options->prefill_chunk_tokens &&
        options->concurrent_sequences < 64ull &&
        (options->compatible_operation_batching == 0 ||
         options->compatible_operation_batching == 1) &&
        (!options->compatible_operation_batching || options->concurrent_sequences > 1ull);
}

static void capacity_options_copy(runtime_capacity_context *context,
                                   const yvex_runtime_capacity_options *options)
{
    context->options = *options;
    if (!context->options.concurrent_sequences)
        context->options.concurrent_sequences = 1ull;
    if (context->options.prefill_chunk_tokens > context->options.context_capacity)
        context->options.prefill_chunk_tokens = context->options.context_capacity;
}
typedef struct {
    yvex_execution_state_class_request classes[YVEX_MODEL_STATE_CLASS_COUNT];
    unsigned long long candidate_bytes_per_token;
} capacity_geometry;

typedef struct {
    const char *identity;
    unsigned long long maximum_context, hidden_width, vocabulary_size;
    unsigned long long residual_streams, candidate_width;
} capacity_semantic_capacity;

static unsigned long long capacity_gcd(unsigned long long left,
                                                   unsigned long long right)
{
    while (right) {
        unsigned long long remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static int capacity_lcm(unsigned long long left,
                                   unsigned long long right,
                                   unsigned long long *result)
{
    unsigned long long divisor;
    if (!left || !right || !result) return 0;
    divisor = capacity_gcd(left, right);
    return yvex_core_u64_mul(left / divisor, right, result);
}

static int capacity_periodic_add(
    yvex_execution_state_class_request *state, unsigned long long period,
    unsigned long long bytes)
{
    unsigned long long common, existing, added;
    if (!state || !period || !bytes) return 0;
    if (!state->bytes_per_block) {
        state->extent = YVEX_EXECUTION_STATE_EXTENT_CONTEXT;
        state->logical_block_tokens = period;
        state->bytes_per_block = bytes;
        return 1;
    }
    if (state->extent != YVEX_EXECUTION_STATE_EXTENT_CONTEXT ||
        !capacity_lcm(state->logical_block_tokens, period, &common) ||
        !yvex_core_u64_mul(state->bytes_per_block,
                           common / state->logical_block_tokens, &existing) ||
        !yvex_core_u64_mul(bytes, common / period, &added) ||
        !yvex_core_u64_add(existing, added, &state->bytes_per_block)) return 0;
    state->logical_block_tokens = common;
    return 1;
}

static int capacity_fixed_add(
    yvex_execution_state_class_request *state, unsigned long long tokens,
    unsigned long long bytes_per_token)
{
    if (!state || !tokens || !bytes_per_token) return 0;
    if (!state->bytes_per_block) {
        state->extent = YVEX_EXECUTION_STATE_EXTENT_FIXED;
        state->logical_block_tokens = 1ull;
        state->fixed_tokens_per_sequence = tokens;
        state->bytes_per_block = bytes_per_token;
        return 1;
    }
    return state->extent == YVEX_EXECUTION_STATE_EXTENT_FIXED &&
           state->fixed_tokens_per_sequence == tokens &&
           yvex_core_u64_add(state->bytes_per_block, bytes_per_token,
                             &state->bytes_per_block);
}

static int capacity_component_bytes(
    const yvex_attention_state_component_recipe *component,
    unsigned long long bank_count, unsigned long long *bytes)
{
    unsigned long long values;
    if (!component || !bytes || !bank_count) return 0;
    if (component->kind == YVEX_ATTENTION_STATE_COMPONENT_HISTORY) {
        if (!yvex_core_u64_mul(component->value_width, sizeof(float), &values) ||
            !yvex_core_u64_add(values, sizeof(unsigned long long), &values) ||
            !yvex_core_u64_mul(values, bank_count, bytes)) return 0;
        return 1;
    }
    if (!yvex_core_u64_add(component->rolling.kv_state_extent,
                           component->rolling.score_state_extent, &values) ||
        !yvex_core_u64_mul(values, sizeof(float), &values) ||
        !yvex_core_u64_mul(values, bank_count, bytes)) return 0;
    return 1;
}

static int capacity_target_component(
    capacity_geometry *geometry,
    const yvex_attention_layer_plan *layer,
    const yvex_attention_state_component_recipe *component)
{
    yvex_model_state_class state_class;
    unsigned long long bytes, candidate;
    if (!capacity_component_bytes(component, 2ull, &bytes) ||
        !capacity_component_bytes(component, 1ull, &candidate) ||
        !yvex_core_u64_add(geometry->candidate_bytes_per_token, candidate,
                           &geometry->candidate_bytes_per_token)) return 0;
    switch (component->binding) {
    case YVEX_ATTENTION_STATE_BINDING_LOCAL_HISTORY:
        if (component->capacity && component->binding ==
                                       YVEX_ATTENTION_STATE_BINDING_LOCAL_HISTORY) {
            if (!yvex_core_u64_mul(bytes, 2ull, &bytes)) return 0;
        }
        return capacity_fixed_add(
            &geometry->classes[YVEX_MODEL_STATE_SWA_RING],
            component->capacity, bytes);
    case YVEX_ATTENTION_STATE_BINDING_COMPRESSED_HISTORY:
        state_class = layer->attention_class == YVEX_ATTENTION_CLASS_HCA
                          ? YVEX_MODEL_STATE_HCA_HISTORY
                          : YVEX_MODEL_STATE_COMPRESSED_HISTORY;
        return capacity_periodic_add(
            &geometry->classes[state_class], layer->compression_ratio, bytes);
    case YVEX_ATTENTION_STATE_BINDING_INDEXER_HISTORY:
        return capacity_periodic_add(
            &geometry->classes[YVEX_MODEL_STATE_INDEXER_HISTORY],
            layer->compression_ratio, bytes);
    case YVEX_ATTENTION_STATE_BINDING_MAIN_ROLLING:
        return capacity_fixed_add(
            &geometry->classes[YVEX_MODEL_STATE_MAIN_ROLLING], 1ull, bytes);
    case YVEX_ATTENTION_STATE_BINDING_INDEXER_ROLLING:
        return capacity_fixed_add(
            &geometry->classes[YVEX_MODEL_STATE_INDEXER_ROLLING], 1ull, bytes);
    default: return 0;
    }
}

static int capacity_plan_accumulate(
    capacity_geometry *geometry,
    const yvex_attention_layer_plan *layers, unsigned long long layer_count,
    const yvex_graph_attention_capacity_plan *capacity, int draft)
{
    unsigned long long layer_index;
    for (layer_index = 0ull; layer_index < layer_count; ++layer_index) {
        const yvex_attention_layer_plan *layer = &layers[layer_index];
        const yvex_graph_attention_capacity_layer *capacity_layer =
            yvex_graph_attention_capacity_plan_layer(capacity, layer_index);
        unsigned int component_index;
        if (!layer || !capacity_layer || !capacity_layer->selected) return 0;
        for (component_index = 0u;
             component_index < capacity_layer->recipe.component_count;
             ++component_index) {
            const yvex_attention_state_component_recipe *component =
                &capacity_layer->recipe.components[component_index];
            unsigned long long bytes;
            if (!draft) {
                if (!capacity_target_component(
                        geometry, layer, component)) return 0;
                continue;
            }
            if (component->kind != YVEX_ATTENTION_STATE_COMPONENT_HISTORY ||
                component->binding != YVEX_ATTENTION_STATE_BINDING_LOCAL_HISTORY ||
                !capacity_component_bytes(component, 2ull, &bytes) ||
                !yvex_core_u64_mul(bytes, 2ull, &bytes) ||
                !capacity_fixed_add(
                    &geometry->classes[YVEX_MODEL_STATE_DRAFT_PERSISTENT],
                    component->capacity, bytes)) return 0;
        }
    }
    return 1;
}

static void capacity_geometry_initialize(
    capacity_geometry *geometry)
{
    unsigned long long index;
    memset(geometry, 0, sizeof(*geometry));
    for (index = 0ull; index < YVEX_MODEL_STATE_CLASS_COUNT; ++index) {
        yvex_execution_state_class_request *state = &geometry->classes[index];
        state->state_class = (yvex_model_state_class)index;
        state->alignment_bytes = 256ull;
        state->kernel_tile_tokens = 1ull;
        state->promotion_granularity_tokens = 1ull;
        state->page_table_entry_bytes = 16ull;
    }
}

static int capacity_graph_geometry(
    runtime_capacity_context *context,
    const capacity_semantic_capacity *semantic,
    capacity_geometry *geometry,
    yvex_graph_attention_capacity_plan **workspace_capacity, yvex_error *err)
{
    const yvex_runtime_binding *binding =
        context && context->model_view
            ? context->model_view->compiled_binding : NULL;
    const yvex_attention_summary *summaries[2];
    const yvex_attention_layer_plan *layers[2];
    unsigned long long layer_counts[2];
    unsigned long long plan_index;
    const yvex_program_physical *program =
        yvex_compiled_model_plan_forward(context->model_view->compiled_plan);
    *workspace_capacity = NULL;
    if (!binding)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled attention geometry is unavailable");
    summaries[0] = &binding->attention;
    summaries[1] = binding->summary.draft_layer_count
                       ? &binding->draft_attention : NULL;
    layers[0] = binding->layers;
    layers[1] = binding->draft_layers;
    layer_counts[0] = binding->summary.layer_count;
    layer_counts[1] = binding->summary.draft_layer_count;
    capacity_geometry_initialize(geometry);
    if ((!layers[0] || !layer_counts[0]) && program) {
        yvex_program_token_interface interface;
        if (yvex_program_physical_token_interface(
                program, &interface, err) != YVEX_OK)
            return yvex_error_code(err);
        if (interface.attention_operations)
            return capacity_context_refuse(
                err, YVEX_ERR_STATE,
                "attention-bearing program has no admitted attention plan");
    }
    for (plan_index = 0ull; plan_index < 2ull; ++plan_index) {
        yvex_graph_attention_capacity_request request = {0};
        yvex_graph_attention_capacity_plan *capacity = NULL;
        int rc;
        if (!summaries[plan_index] || !layers[plan_index] ||
            !layer_counts[plan_index]) {
            if (!plan_index && !program)
                return capacity_context_refuse(
                    err, YVEX_ERR_STATE,
                    "model state geometry requires an unavailable attention plan");
            continue;
        }
        request.scope = YVEX_ATTENTION_PROBE_SCOPE_FULL;
        request.history_tokens = 0ull;
        request.start_position = 0ull;
        request.token_count = context->options.context_capacity;
        request.execution_count = 1ull;
        request.use_requested_position = 1;
        rc = yvex_graph_attention_capacity_plan_build_compiled(
            &capacity, summaries[plan_index], layers[plan_index],
            layer_counts[plan_index], &request, err);
        if (rc == YVEX_OK &&
            !capacity_plan_accumulate(
                geometry, layers[plan_index], layer_counts[plan_index],
                capacity, plan_index != 0ull))
            rc = capacity_context_refuse(
                err, YVEX_ERR_BOUNDS,
                "state-class geometry cannot represent the admitted graph plan");
        if (rc == YVEX_OK && !plan_index) {
            *workspace_capacity = capacity;
            capacity = NULL;
        }
        yvex_graph_attention_capacity_plan_close(&capacity);
        if (rc != YVEX_OK) return rc;
    }
    if (program) {
        yvex_sequence_state_plan required;
        yvex_sequence_state_geometry state;
        unsigned long long bytes;
        if (!yvex_program_physical_sequence_state(program, &required))
            return capacity_context_refuse(err, YVEX_ERR_STATE,
                "compiled recurrent state has no physical provider bindings");
        int rc = yvex_sequence_state_plan_measure(&required, &state, err);
        if (rc != YVEX_OK) return rc;
        if (!yvex_core_u64_add(state.committed_bytes, state.candidate_bytes, &bytes) ||
            (bytes &&
            !capacity_fixed_add(
                &geometry->classes[YVEX_MODEL_STATE_RECURRENT_SEQUENCE],
                1ull, bytes)))
            return capacity_context_refuse(
                err, YVEX_ERR_BOUNDS,
                "recurrent sequence-state geometry overflowed");
    }
    if (semantic->residual_streams > 1ull) {
        unsigned long long bytes;
        if (!yvex_core_u64_mul(semantic->residual_streams,
                               semantic->hidden_width, &bytes) ||
            !yvex_core_u64_mul(bytes, sizeof(float), &bytes) ||
            !capacity_fixed_add(
                &geometry->classes[YVEX_MODEL_STATE_RESIDUAL_MIXING], 1ull, bytes))
            return capacity_context_refuse(
                err, YVEX_ERR_BOUNDS,
                "residual state geometry overflowed");
    }
    if (geometry->candidate_bytes_per_token) {
        yvex_execution_state_class_request *candidate =
            &geometry->classes[YVEX_MODEL_STATE_CANDIDATE_DELTA];
        candidate->extent = YVEX_EXECUTION_STATE_EXTENT_CANDIDATE;
        candidate->logical_block_tokens = 1ull;
        candidate->bytes_per_block = geometry->candidate_bytes_per_token;
        candidate->promotion_granularity_tokens =
            semantic->candidate_width;
    }
    if (geometry->candidate_bytes_per_token) {
        yvex_execution_state_class_request *prefix =
            &geometry->classes[YVEX_MODEL_STATE_PREFIX_CHECKPOINT];
        prefix->extent = YVEX_EXECUTION_STATE_EXTENT_PREFIX_BUDGET;
        prefix->logical_block_tokens = 1ull;
        prefix->bytes_per_block = 16ull;
        prefix->kernel_tile_tokens = semantic->candidate_width;
        prefix->shared = 1;
        prefix->copy_on_write = 1;
    }
    return YVEX_OK;
}

static int capacity_semantic_capacity_build(
    runtime_capacity_context *context,
    capacity_semantic_capacity *semantic, yvex_error *err)
{
    const yvex_transformer_plan_summary *transformer =
        yvex_transformer_plan_summary_get(
            yvex_compiled_model_plan_transformer(
                context->model_view->compiled_plan, 0));
    const yvex_program_physical *program =
        yvex_compiled_model_plan_forward(context->model_view->compiled_plan);
    const yvex_speculation_family_policy *speculation = NULL;

    memset(semantic, 0, sizeof(*semantic));
    if (!context->model_view->binding || !context->model_view->compiled_plan ||
        !yvex_runtime_binding_policies(
            context->model_view->compiled_binding, NULL, NULL, &speculation))
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "execution requires one sealed semantic producer");
    semantic->identity =
        context->model_view->binding->model_execution_identity;
    semantic->maximum_context =
        context->model_view->binding->semantic_maximum_context;
    if (!yvex_sha256_hex_valid(semantic->identity) ||
        !semantic->maximum_context)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled semantic context capability is unavailable");
    if (context->options.context_capacity > semantic->maximum_context)
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "requested context exceeds the model-authored semantic maximum");
    if (!transformer && !program)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled model exposes no executable token producer");
    if (transformer && program)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled model exposes ambiguous token producers");
    if (program) {
        yvex_program_token_interface signature;
        int rc = yvex_program_physical_token_interface(program, &signature, err);
        if (rc != YVEX_OK) return rc;
        if (context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE)
            return capacity_context_refuse(
                err, YVEX_ERR_UNSUPPORTED,
                "decoder execution has no admitted draft producer");
        semantic->hidden_width = signature.hidden_width;
        semantic->vocabulary_size = signature.vocabulary_size;
        semantic->residual_streams = 1ull;
        semantic->candidate_width = 1ull;
    } else {
        semantic->hidden_width = transformer->hidden_width;
        semantic->vocabulary_size = transformer->vocabulary_size;
        semantic->residual_streams = transformer->residual_streams;
        semantic->candidate_width = speculation && speculation->block_size
                                        ? speculation->block_size + 1ull
                                        : 1ull;
    }
    return YVEX_OK;
}

static int capacity_hardware(
    runtime_capacity_context *context, yvex_backend *backend,
    yvex_runtime_weight_placement placement, int resident,
    unsigned long long *live_available, yvex_error *err)
{
    yvex_backend_device_info device = {0};
    yvex_backend_cuda_attention_graph_summary cuda = {0};
    const char *placement_name = "host";
    long page_bytes;
    unsigned long long system_total, total, available, reserve_basis;
    int process_limited, shared_system_domain = 0, rc;
    if (!context || !live_available ||
        (context->options.backend == YVEX_BACKEND_KIND_CUDA && !backend) ||
        (backend && yvex_backend_get_device_info(backend, &device, err) != YVEX_OK) ||
        (page_bytes = sysconf(_SC_PAGESIZE)) <= 0)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "memory-admission hardware facts are unavailable");
    if (!backend) device.kind = YVEX_BACKEND_KIND_CPU;
    if (device.kind != context->options.backend)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "capacity backend differs from the requested execution backend");
    if (!yvex_runtime_private_memory_capacity(
            &system_total, &available, &process_limited))
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "live process memory capacity is unavailable");
    if (device.kind == YVEX_BACKEND_KIND_CUDA) {
        shared_system_domain =
            device.unified_addressing && device.total_memory_bytes == system_total &&
            ((placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_CUDA_MANAGED &&
              device.managed_memory) ||
             (placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_ARTIFACT_MAPPED &&
              (resident ||
               yvex_backend_resident_map_readonly_supported(backend))));
        total = shared_system_domain ? system_total : device.total_memory_bytes;
        if (!shared_system_domain && device.free_memory_bytes < available)
            available = device.free_memory_bytes;
    } else {
        total = system_total;
    }
    *live_available = available;
    memset(&context->result.hardware_profile, 0, sizeof(context->result.hardware_profile));
    context->result.hardware_profile.schema_version =
        YVEX_EXECUTION_HARDWARE_PROFILE_SCHEMA_V1;
    context->result.hardware_profile.backend = device.kind;
    context->result.hardware_profile.admitted_fact_mask =
        YVEX_EXECUTION_HARDWARE_FACT_BIT(YVEX_EXECUTION_HARDWARE_FACT_MEMORY) |
        YVEX_EXECUTION_HARDWARE_FACT_BIT(YVEX_EXECUTION_HARDWARE_FACT_PAGING);
    context->result.hardware_profile.device_index = backend ? device.device_index : 0;
    context->result.hardware_profile.compute_major = device.compute_capability_major;
    context->result.hardware_profile.compute_minor = device.compute_capability_minor;
    context->result.hardware_profile.total_memory_bytes = total;
    context->result.hardware_profile.usable_memory_bytes = total;
    if (device.kind == YVEX_BACKEND_KIND_CUDA &&
        context->options.maximum_device_bytes &&
        context->options.maximum_device_bytes <
            context->result.hardware_profile.usable_memory_bytes)
        context->result.hardware_profile.usable_memory_bytes =
            context->options.maximum_device_bytes;
    if ((device.kind == YVEX_BACKEND_KIND_CPU ||
         placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_CUDA_MANAGED ||
         placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_ARTIFACT_MAPPED) &&
        context->options.maximum_host_bytes &&
        context->options.maximum_host_bytes <
            context->result.hardware_profile.usable_memory_bytes)
        context->result.hardware_profile.usable_memory_bytes =
            context->options.maximum_host_bytes;
    reserve_basis = system_total;
    if (context->options.maximum_host_bytes &&
        context->options.maximum_host_bytes < reserve_basis)
        reserve_basis = context->options.maximum_host_bytes;
    context->result.system_capacity_bytes = reserve_basis;
    context->result.system_reserve_bytes =
        yvex_runtime_private_system_reserve(reserve_basis);
    context->result.hardware_profile.host_page_bytes = (unsigned long long)page_bytes;
    context->result.hardware_profile.device_page_bytes = (unsigned long long)page_bytes;
    context->result.hardware_profile.unified_addressing = device.unified_addressing;
    context->result.hardware_profile.coherent_host_memory = shared_system_domain;
    if (device.kind == YVEX_BACKEND_KIND_CUDA && resident) {
        if (yvex_backend_cuda_attention_graph_summary_get(
                backend, &cuda, err) != YVEX_OK ||
            !cuda.kernel_bundle_architecture[0] ||
            !yvex_sha256_hex_valid(cuda.cuda_build_identity))
            return capacity_context_refuse(
                err, YVEX_ERR_STATE,
                "kernel-bundle hardware facts are unavailable");
        rc = yvex_backend_bandwidth_probe(
            backend, &context->result.bandwidth_evidence, err);
        if (rc != YVEX_OK) return rc;
        if (placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_CUDA_MANAGED) {
            context->result.hardware_profile.sustainable_read_bytes_per_second =
                context->result.bandwidth_evidence.sustainable_read_bytes_per_second;
            placement_name = "managed";
        } else if (placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_ARTIFACT_MAPPED) {
            context->result.hardware_profile.sustainable_read_bytes_per_second =
                context->result.bandwidth_evidence.sustainable_coherent_host_bytes_per_second;
            placement_name = "artifact-map";
        } else if (placement == YVEX_RUNTIME_WEIGHT_PLACEMENT_HOST_LOCKED) {
            context->result.hardware_profile.sustainable_read_bytes_per_second =
                context->result.bandwidth_evidence.sustainable_coherent_host_bytes_per_second;
            placement_name = "hostmap";
        } else {
            return capacity_context_refuse(
                err, YVEX_ERR_STATE,
                "model residency has no admitted bandwidth placement");
        }
        context->result.hardware_profile.sustainable_copy_bytes_per_second =
            context->result.bandwidth_evidence.sustainable_copy_bytes_per_second;
        context->result.hardware_profile.admitted_fact_mask |=
            YVEX_EXECUTION_HARDWARE_FACT_BIT(
                YVEX_EXECUTION_HARDWARE_FACT_BANDWIDTH);
        context->result.hardware_profile.native_architecture_code =
            cuda.kernel_bundle_native;
        if (cuda.kernel_bundle_native)
            context->result.hardware_profile.admitted_fact_mask |=
                YVEX_EXECUTION_HARDWARE_FACT_BIT(
                    YVEX_EXECUTION_HARDWARE_FACT_NATIVE_CODE);
        (void)snprintf(context->result.hardware_profile.name,
                       sizeof(context->result.hardware_profile.name),
                       "cuda-%s-%s", cuda.kernel_bundle_architecture,
                       placement_name);
    } else if (device.kind == YVEX_BACKEND_KIND_CUDA) {
        yvex_core_text_copy(context->result.hardware_profile.name,
                            sizeof(context->result.hardware_profile.name),
                            "cuda-capacity-preflight");
    } else {
        yvex_core_text_copy(context->result.hardware_profile.name,
                            sizeof(context->result.hardware_profile.name),
                            "cpu-memory");
    }
    (void)process_limited;
    return yvex_execution_hardware_profile_seal(
        &context->result.hardware_profile, err);
}

static int capacity_workload(
    runtime_capacity_context *context, yvex_error *err)
{
    static const char *const names[] = {
        "interactive-latency", "balanced-serving", "long-context",
        "deep-context", "full-model-research"
    };
    const yvex_speculation_family_policy *speculation = NULL;
    if (context && context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE &&
        context->model_view && context->model_view->compiled_binding &&
        !yvex_runtime_binding_policies(
            context->model_view->compiled_binding, NULL, NULL, &speculation))
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled speculation workload geometry is unavailable");
    memset(&context->result.workload_profile, 0, sizeof(context->result.workload_profile));
    context->result.workload_profile.schema_version =
        YVEX_EXECUTION_WORKLOAD_PROFILE_SCHEMA_V1;
    context->result.workload_profile.kind = context->options.workload_kind;
    context->result.workload_profile.minimum_session_context =
        context->options.context_capacity;
    context->result.workload_profile.requested_session_context =
        context->options.context_capacity;
    context->result.workload_profile.concurrent_sequences =
        context->options.concurrent_sequences;
    context->result.workload_profile.logical_batch_tokens =
        context->options.prefill_chunk_tokens;
    context->result.workload_profile.prefill_chunk_tokens =
        context->options.prefill_chunk_tokens;
    context->result.workload_profile.attention_microbatch_rows =
        context->options.prefill_chunk_tokens;
    context->result.workload_profile.moe_row_tile =
        context->options.prefill_chunk_tokens;
    context->result.workload_profile.output_head_rows =
        speculation ? speculation->block_size + 1ull
                    : 1ull;
    if (!context->result.system_capacity_bytes || !context->result.system_reserve_bytes)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "runtime system-reserve capacity is unavailable");
    context->result.workload_profile.system_reserve_bytes =
        context->result.system_reserve_bytes;
    context->result.workload_profile.latency_priority =
        context->options.workload_kind ==
        YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY;
    /* Compatible-operation coalescing is not dynamic continuous batching. */
    context->result.workload_profile.continuous_batching = 0;
    yvex_core_text_copy(context->result.workload_profile.name,
                        sizeof(context->result.workload_profile.name),
                        names[context->options.workload_kind]);
    return yvex_execution_workload_profile_seal(
        &context->result.workload_profile, err);
}

static int capacity_physical_row_capacity(
    const runtime_capacity_context *context,
    unsigned long long *capacity, yvex_error *err)
{
    const yvex_speculation_family_policy *speculation = NULL;
    unsigned long long draft_width = 0ull;
    if (capacity) *capacity = 0ull;
    if (!context || !capacity || !context->options.prefill_chunk_tokens ||
        !context->model_view || !context->model_view->compiled_binding ||
        !yvex_runtime_binding_policies(
            context->model_view->compiled_binding, NULL, NULL, &speculation))
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled physical row geometry is unavailable");
    *capacity = context->options.prefill_chunk_tokens;
    if (context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE) {
        if (!speculation ||
            !yvex_core_u64_add(speculation->block_size, 2ull, &draft_width))
            return capacity_context_refuse(
                err, YVEX_ERR_BOUNDS,
                "compiled speculative execution width is invalid");
        if (draft_width > *capacity) *capacity = draft_width;
    }
    if (context->options.compatible_operation_batching &&
        context->options.concurrent_sequences > *capacity)
        *capacity = context->options.concurrent_sequences;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int capacity_sampling_workspace(
    const runtime_capacity_context *context, yvex_backend *backend,
    unsigned long long vocabulary_size, unsigned long long proposal_width,
    unsigned long long *workspace, yvex_error *err)
{
    const yvex_backend_sampling_operations *operations;
    unsigned long long selection = 0ull, speculation = 0ull;
    if (workspace) *workspace = 0ull;
    if (!context || !workspace)
        return capacity_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "device sampling workspace owner is unavailable");
    if (context->options.backend != YVEX_BACKEND_KIND_CUDA ||
        context->options.evidence_profile != YVEX_EXECUTION_EVIDENCE_PRODUCTION ||
        context->options.sampling_requirement != YVEX_EXECUTION_SAMPLING_STOCHASTIC ||
        !yvex_backend_sampling_operations_get(backend)) return YVEX_OK;
    operations = yvex_backend_sampling_operations_get(backend);
    if (!vocabulary_size || !operations || !operations->workspace_required ||
        operations->workspace_required(
            vocabulary_size, &selection, err) != YVEX_OK)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "device stochastic workspace geometry is unavailable");
    if (context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE &&
        (!proposal_width ||
         !operations->speculation_workspace_required ||
         operations->speculation_workspace_required(
             vocabulary_size, proposal_width,
             &speculation, err) != YVEX_OK))
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "device stochastic speculation workspace geometry is unavailable");
    *workspace = speculation > selection ? speculation : selection;
    return YVEX_OK;
}

static int capacity_moe_workspace(
    const runtime_capacity_context *context, yvex_backend *backend,
    unsigned long long target_rows, unsigned long long draft_rows,
    unsigned long long *workspace, yvex_error *err)
{
    const yvex_backend_moe_operations *operations;
    unsigned long long layer_max = 0ull, maximum_layers = 0ull;
    unsigned long long completion_bytes;
    unsigned int draft;
    if (workspace) *workspace = 0ull;
    if (!context || !workspace || !target_rows)
        return capacity_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "compiled MoE workspace facts are incomplete");
    if (context->options.backend != YVEX_BACKEND_KIND_CUDA) return YVEX_OK;
    operations = yvex_backend_moe_operations_get(backend);
    for (draft = 0u; draft < 2u; ++draft) {
        const yvex_moe_plan *plan = yvex_compiled_model_plan_moe(
            context->model_view->compiled_plan, draft != 0u);
        const yvex_moe_plan_summary *summary = yvex_moe_plan_summary_get(plan);
        unsigned long long rows = draft ? draft_rows : target_rows;
        unsigned long long index;
        if (!summary || !rows) continue;
        if (!operations || !operations->workspace_required)
            return capacity_context_refuse(
                err, YVEX_ERR_UNSUPPORTED,
                "CUDA MoE workspace capability is unavailable");
        if (summary->layer_count > maximum_layers)
            maximum_layers = summary->layer_count;
        for (index = 0ull; index < summary->layer_count; ++index) {
            unsigned long long bytes;
            int rc = operations->workspace_required(
                yvex_moe_plan_layer_at(plan, index), rows, &bytes, err);
            if (rc != YVEX_OK) return rc;
            if (bytes > layer_max) layer_max = bytes;
        }
    }
    if (!maximum_layers) return YVEX_OK;
    if (!yvex_core_u64_mul(
            maximum_layers, sizeof(yvex_moe_device_completion_slot),
            &completion_bytes) || !layer_max ||
        !yvex_core_u64_add(layer_max, completion_bytes, workspace))
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "CUDA MoE workspace extent overflowed");
    return YVEX_OK;
}

static int capacity_decoder_attention_workspace(
    const runtime_capacity_context *context, yvex_backend *backend,
    unsigned long long *workspace, yvex_error *err)
{
    const yvex_backend_transformer_operations *operations =
        yvex_backend_transformer_operations_get(backend);
    const yvex_runtime_binding *binding = context->model_view->compiled_binding;
    unsigned long long query_tokens = context->options.prefill_chunk_tokens;
    unsigned long long index;

    *workspace = 0ull;
    if (!operations || !operations->attention_workspace_required || !binding ||
        !binding->layers || !binding->summary.layer_count)
        return capacity_context_refuse(
            err, YVEX_ERR_UNSUPPORTED,
            "decoder exact-attention workspace capability is unavailable");
    if (query_tokens > context->options.context_capacity)
        query_tokens = context->options.context_capacity;
    for (index = 0ull; index < binding->summary.layer_count; ++index) {
        const yvex_attention_layer_plan *layer = &binding->layers[index];
        yvex_transformer_attention_requirement requirement = {
            .query_tokens = query_tokens,
            .key_value_tokens = context->options.context_capacity,
            .query_start = context->options.context_capacity - query_tokens,
            .query_heads = layer->query_heads,
            .key_value_heads = layer->kv_heads,
            .head_dimension = layer->head_dimension,
            .query_dtype = YVEX_DTYPE_F32,
            .key_dtype = YVEX_DTYPE_F32,
            .value_dtype = YVEX_DTYPE_F32,
            .output_dtype = YVEX_DTYPE_F32,
            .layout = YVEX_TRANSFORMER_ATTENTION_LAYOUT_TOKEN_HEAD_DIM,
            .mask = YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL,
            .numeric_contract = YVEX_TRANSFORMER_ATTENTION_NUMERIC_EXACT_F32,
            .deterministic = 1};
        unsigned long long bytes;
        int rc = operations->attention_workspace_required(
            &requirement, &bytes, err);
        if (rc != YVEX_OK) return rc;
        if (bytes > *workspace) *workspace = bytes;
    }
    return YVEX_OK;
}

static int capacity_attention_workspace(
    const runtime_capacity_context *context,
    yvex_backend *backend,
    const yvex_graph_attention_capacity_plan *capacity,
    unsigned long long physical_rows, unsigned long long *workspace,
    yvex_error *err)
{
    const yvex_runtime_binding *binding =
        context ? context->model_view->compiled_binding : NULL;
    const yvex_attention_summary *summaries[2];
    const yvex_attention_layer_plan *layers[2];
    unsigned long long layer_counts[2], plan_count, plan_index;
    int deferred = context &&
        context->options.backend == YVEX_BACKEND_KIND_CUDA &&
        runtime_attention_evidence(context->options.evidence_profile) ==
            YVEX_ATTENTION_EVIDENCE_NONE;
    if (workspace) *workspace = 0ull;
    if (context && yvex_compiled_model_plan_forward(
                       context->model_view->compiled_plan)) {
        yvex_program_token_interface interface;
        int rc = yvex_program_physical_token_interface(
            yvex_compiled_model_plan_forward(
                context->model_view->compiled_plan),
            &interface, err);
        if (rc != YVEX_OK) return rc;
        if (!interface.attention_operations) return YVEX_OK;
        return capacity_decoder_attention_workspace(
            context, backend, workspace, err);
    }
    if (!binding || !capacity || !physical_rows || !workspace)
        return capacity_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "compiled attention workspace facts are incomplete");
    summaries[0] = &binding->attention;
    summaries[1] = &binding->draft_attention;
    layers[0] = binding->layers;
    layers[1] = binding->draft_layers;
    layer_counts[0] = binding->summary.layer_count;
    layer_counts[1] = binding->summary.draft_layer_count;
    plan_count = context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE ? 2ull : 1ull;
    for (plan_index = 0ull; plan_index < plan_count; ++plan_index) {
        yvex_graph_attention_capacity_plan *owned_capacity = NULL;
        const yvex_graph_attention_capacity_plan *selected_capacity = capacity;
        unsigned int mode;
        int rc = YVEX_OK;
        if (!summaries[plan_index] || !layers[plan_index] ||
            !layer_counts[plan_index])
            return capacity_context_refuse(
                err, YVEX_ERR_STATE,
                "compiled attention workspace plan is unavailable");
        if (plan_index) {
            yvex_graph_attention_capacity_request request = {
                .scope = YVEX_ATTENTION_PROBE_SCOPE_FULL,
                .token_count = context->options.context_capacity,
                .execution_count = 1ull,
                .use_requested_position = 1,
            };
            rc = yvex_graph_attention_capacity_plan_build_compiled(
                &owned_capacity, summaries[plan_index], layers[plan_index],
                layer_counts[plan_index], &request, err);
            selected_capacity = owned_capacity;
        }
        for (mode = YVEX_ATTENTION_EXECUTION_EAGER;
             rc == YVEX_OK && mode <= YVEX_ATTENTION_EXECUTION_FULL; ++mode) {
            unsigned long long bytes;
            rc = yvex_runtime_private_attention_workspace_required(
                summaries[plan_index], layers[plan_index],
                layer_counts[plan_index], selected_capacity,
                (yvex_attention_execution_mode)mode,
                YVEX_ATTENTION_OPERATION_ENVELOPE,
                runtime_attention_evidence(context->options.evidence_profile),
                physical_rows, deferred, &bytes, err);
            if (rc == YVEX_OK && bytes > *workspace) *workspace = bytes;
        }
        yvex_graph_attention_capacity_plan_close(&owned_capacity);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

static int capacity_build_for(
    runtime_capacity_context *context, yvex_backend *backend,
    yvex_runtime_weight_placement placement, unsigned long long model_bytes,
    int model_resident, unsigned long long transient_bytes,
    yvex_graph_attention_capacity_plan **workspace_capacity,
    unsigned long long *required_out, unsigned long long *available_out,
    yvex_error *err)
{
    capacity_semantic_capacity semantic;
    yvex_compiled_context_envelope context_envelope;
    capacity_geometry geometry;
    yvex_execution_state_class_request states[YVEX_MODEL_STATE_CLASS_COUNT];
    yvex_execution_capacity_plan_request request = {0};
    unsigned long long workspace, sampling_workspace = 0ull;
    unsigned long long attention_workspace = 0ull, moe_workspace = 0ull;
    unsigned long long physical_rows, draft_rows = 0ull, index, count = 0ull;
    unsigned long long graph_bytes, scheduler_bytes, live_available, live_required;
    int rc;
    if (required_out) *required_out = 0ull;
    if (available_out) *available_out = 0ull;
    if (capacity_hardware(
            context, backend, placement, model_resident,
            &live_available, err) != YVEX_OK)
        return yvex_error_code(err);
    if (available_out) *available_out = live_available;
    if (capacity_semantic_capacity_build(context, &semantic, err) != YVEX_OK)
        return yvex_error_code(err);
    if (capacity_workload(context, err) != YVEX_OK) return yvex_error_code(err);
    if (yvex_compiled_model_plan_context_envelope(
            context->model_view->compiled_plan, semantic.identity,
            semantic.maximum_context, &context_envelope, err) != YVEX_OK ||
        yvex_compiled_context_envelope_admit(
            &context_envelope, context->options.context_capacity,
            context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE, err) != YVEX_OK)
        return yvex_error_code(err);
    if (capacity_graph_geometry(
            context, &semantic, &geometry, workspace_capacity, err) != YVEX_OK)
        return yvex_error_code(err);
    if (capacity_physical_row_capacity(
            context, &physical_rows, err) != YVEX_OK)
        return yvex_error_code(err);
    context->result.physical_rows = physical_rows;
    if (context->options.mode == YVEX_EXECUTION_GENERATION_SPECULATIVE &&
        !yvex_core_u64_add(semantic.candidate_width, 1ull, &draft_rows))
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "compiled draft workspace row extent overflowed");
    for (index = 0ull; index < YVEX_MODEL_STATE_CLASS_COUNT; ++index) {
        if (!geometry.classes[index].bytes_per_block) continue;
        states[count++] = geometry.classes[index];
        request.semantic_state_class_mask |= YVEX_MODEL_STATE_CLASS_BIT(index);
    }
    if (!count)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "compiled graph exposes no persistent-state geometry");
    if (!yvex_core_u64_mul(context->options.prefill_chunk_tokens,
                           semantic.hidden_width, &workspace) ||
        !yvex_core_u64_mul(workspace, sizeof(float) * 8ull, &workspace) ||
        !workspace)
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "execution workspace geometry overflowed");
    if (capacity_sampling_workspace(
            context, backend, semantic.vocabulary_size,
            semantic.candidate_width ? semantic.candidate_width - 1ull : 0ull,
            &sampling_workspace, err) != YVEX_OK)
        return yvex_error_code(err);
    context->result.sampling_workspace_bytes = sampling_workspace;
    if (sampling_workspace > workspace) workspace = sampling_workspace;
    if (capacity_attention_workspace(
            context, backend, *workspace_capacity, physical_rows,
            &attention_workspace, err) != YVEX_OK ||
        capacity_moe_workspace(
            context, backend, context->options.prefill_chunk_tokens,
            draft_rows, &moe_workspace, err) != YVEX_OK)
        return yvex_error_code(err);
    if (attention_workspace > workspace) workspace = attention_workspace;
    if (moe_workspace > workspace) workspace = moe_workspace;
    request.schema_version = YVEX_EXECUTION_CAPACITY_PLAN_SCHEMA_V1;
    request.model_execution_identity = semantic.identity;
    request.semantic_maximum_context = semantic.maximum_context;
    request.candidate_width = semantic.candidate_width;
    request.hardware = &context->result.hardware_profile;
    request.workload = &context->result.workload_profile;
    request.model_bytes = model_bytes;
    request.state_classes = states;
    request.state_class_count = count;
    if (!yvex_core_u64_mul(workspace,
                           context->result.workload_profile.concurrent_sequences,
                           &request.workspace_bytes) ||
        !yvex_core_u64_mul(1024ull * 1024ull,
                           context->result.workload_profile.concurrent_sequences,
                           &graph_bytes) ||
        !yvex_core_u64_mul(1024ull * 1024ull,
                           context->result.workload_profile.concurrent_sequences,
                           &scheduler_bytes))
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "multi-sequence fixed-resource accounting overflowed");
    request.scheduler_bytes = scheduler_bytes;
    request.graph_bytes = graph_bytes;
    if (!request.model_bytes)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "resident model byte extent is unavailable for capacity admission");
    rc = yvex_execution_capacity_plan_build(
        &request, &context->result.capacity_plan, err);
    if (rc != YVEX_OK) return rc;
    if (context->result.capacity_plan.required_bytes < request.model_bytes)
        return capacity_context_refuse(
            err, YVEX_ERR_STATE,
            "capacity plan does not cover resident model bytes");
    live_required = context->result.capacity_plan.required_bytes;
    if (model_resident)
        live_required -= request.model_bytes;
    else if (!yvex_core_u64_add(live_required, transient_bytes,
                                &live_required))
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "transient admission peak accounting overflowed");
    if (required_out) *required_out = live_required;
    if (live_required > live_available)
        return capacity_context_refuse(
            err, YVEX_ERR_BOUNDS,
            model_resident
                ? "live process memory cannot preserve the admitted runtime reserve"
                : "pre-residency peak cannot preserve the admitted runtime reserve");
    return YVEX_OK;
}


int yvex_runtime_capacity_derive(
    yvex_model_engine *model, yvex_runtime_execution_session *session,
    const yvex_runtime_capacity_options *options, yvex_runtime_capacity *out,
    yvex_graph_attention_capacity_plan **attention_capacity, yvex_error *err)
{
    runtime_capacity_context context = {0};
    const yvex_runtime_session_view *view = yvex_runtime_session_view_get(session);
    yvex_runtime_session_summary summary = {0};
    yvex_model_engine_summary engine = {0};
    yvex_runtime_residency_summary residency = {0};
    unsigned long long model_bytes;
    int rc;
    if (out) memset(out, 0, sizeof(*out));
    if (attention_capacity) *attention_capacity = NULL;
    if (!out || !attention_capacity || !capacity_options_valid(options) ||
        !view || view->engine != model || !view->backend ||
        !(context.model_view = yvex_model_engine_view_get(model)) ||
        !context.model_view->residency ||
        yvex_model_engine_summary_copy(model, &engine, err) != YVEX_OK ||
        yvex_runtime_session_summary_copy(session, &summary, err) != YVEX_OK ||
        !engine.valid || !engine.sealed || !summary.open || summary.busy ||
        summary.invalidated || session->closing ||
        summary.backend != options->backend ||
        summary.engine_generation != engine.engine_generation)
        return capacity_context_refuse(err, YVEX_ERR_STATE,
            "current paired engine and session capacity facts are required");
    rc = yvex_runtime_residency_snapshot(
        context.model_view->residency, &residency, NULL, NULL, err);
    if (rc != YVEX_OK) return rc;
    if (!residency.encoded_bytes)
        return capacity_context_refuse(err, YVEX_ERR_STATE,
            "model residency placement facts are unavailable");
    capacity_options_copy(&context, options);
    model_bytes = residency.cuda_addressable_bytes
                      ? residency.cuda_addressable_bytes
                      : residency.host_resident_bytes
                            ? residency.host_resident_bytes
                            : residency.mapped_package_bytes
                                  ? residency.mapped_package_bytes
                                  : residency.encoded_bytes;
    rc = capacity_build_for(&context, view->backend, residency.placement,
        model_bytes, 1, 0ull, attention_capacity, NULL, NULL, err);
    if (rc == YVEX_OK) *out = context.result;
    else yvex_graph_attention_capacity_plan_close(attention_capacity);
    return rc;
}

int yvex_runtime_capacity_preflight(
    const yvex_runtime_binding *binding, yvex_backend *backend,
    const yvex_runtime_capacity_options *options,
    unsigned long long *required_bytes, unsigned long long *available_bytes,
    yvex_error *err)
{
    runtime_capacity_context context = {0};
    yvex_model_engine_view view = {0};
    yvex_graph_attention_capacity_plan *workspace_capacity = NULL;
    yvex_runtime_weight_placement placement;
    unsigned long long transient_bytes, model_bytes;
    int rc;
    if (required_bytes) *required_bytes = 0ull;
    if (available_bytes) *available_bytes = 0ull;
    if (!binding || !capacity_options_valid(options) ||
        !required_bytes || !available_bytes ||
        !runtime_binding_maximum_tensor_bytes(
            binding, &transient_bytes))
        return capacity_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "complete startup capacity facts are required");
    view.binding = &binding->summary;
    view.compiled_binding = binding;
    view.compiled_plan = binding->plan;
    context.model_view = &view;
    capacity_options_copy(&context, options);
    rc = yvex_runtime_private_weight_placement_select(
        binding, options->backend, backend, &placement, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_private_residency_backing_bytes(
            binding, backend, placement, &model_bytes, err);
    if (rc == YVEX_OK)
        rc = capacity_build_for(
            &context, backend, placement, model_bytes,
            0, transient_bytes, &workspace_capacity, required_bytes,
            available_bytes, err);
    yvex_graph_attention_capacity_plan_close(&workspace_capacity);
    return rc;
}
