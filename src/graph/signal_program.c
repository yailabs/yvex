/* Compile source-declared convolutions, alias-free activations and residual
 * branches. No runtime buffers, payload inspection or backend topology. */
#include <yvex/internal/signal_program.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/qtype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_ir_id value;
    unsigned long long channels, length;
} signal_value;

typedef struct {
    yvex_ir_module *module;
    yvex_ir_id block;
    yvex_signal_program *program;
    const yvex_alias_decoder_recipe *recipe;
    const char *identity;
    const char *entrypoint;
    yvex_alias_decoder_weight_name_fn name;
    void *name_context;
    yvex_program_parameter_binding *bindings;
    size_t capacity;
    unsigned long long batch;
    yvex_error *err;
    int rc;
} signal_builder;

static int signal_program_refuse(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.signal", message);
    return YVEX_ERR_FORMAT;
}

int yvex_alias_decoder_template_name(
    void *context, yvex_alias_decoder_weight_role role,
    unsigned long long stage, unsigned long long block,
    unsigned long long layer, char output[256], yvex_error *err)
{
    const yvex_alias_decoder_name_templates *templates =
        (const yvex_alias_decoder_name_templates *)context;
    int length = -1;
    unsigned long long residual = 0u;
    if (!templates || role >= YVEX_ALIAS_DECODER_WEIGHT_ROLE_COUNT || !output ||
        !templates->input_projection || !templates->pre_convolution ||
        !templates->upsamples || !templates->residual_blocks ||
        !templates->post_activation || !templates->post_convolution ||
        !templates->blocks_per_stage || block >= templates->blocks_per_stage ||
        !yvex_core_u64_mul(stage, templates->blocks_per_stage, &residual) ||
        !yvex_core_u64_add(residual, block, &residual)) {
        return signal_program_refuse(err, "alias decoder weight-name template is incomplete");
    }
    switch (role) {
    case YVEX_ALIAS_DECODER_INPUT_WEIGHT:
        length = snprintf(output, 256u, "%s.weight", templates->input_projection); break;
    case YVEX_ALIAS_DECODER_INPUT_BIAS:
        length = snprintf(output, 256u, "%s.bias", templates->input_projection); break;
    case YVEX_ALIAS_DECODER_PRE_WEIGHT:
        length = snprintf(output, 256u, "%s.weight_v", templates->pre_convolution); break;
    case YVEX_ALIAS_DECODER_PRE_GAIN:
        length = snprintf(output, 256u, "%s.weight_g", templates->pre_convolution); break;
    case YVEX_ALIAS_DECODER_PRE_BIAS:
        length = snprintf(output, 256u, "%s.bias", templates->pre_convolution); break;
    case YVEX_ALIAS_DECODER_UP_WEIGHT:
        length = snprintf(output, 256u, "%s.%llu.0.weight_v", templates->upsamples, stage); break;
    case YVEX_ALIAS_DECODER_UP_GAIN:
        length = snprintf(output, 256u, "%s.%llu.0.weight_g", templates->upsamples, stage); break;
    case YVEX_ALIAS_DECODER_UP_BIAS:
        length = snprintf(output, 256u, "%s.%llu.0.bias", templates->upsamples, stage); break;
    case YVEX_ALIAS_DECODER_ACT_ALPHA:
        length = snprintf(output, 256u, "%s.%llu.activations.%llu.act.alpha",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_ACT_BETA:
        length = snprintf(output, 256u, "%s.%llu.activations.%llu.act.beta",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_ACT_UP_FILTER:
        length = snprintf(output, 256u, "%s.%llu.activations.%llu.upsample.filter",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_ACT_DOWN_FILTER:
        length = snprintf(output, 256u, "%s.%llu.activations.%llu.downsample.lowpass.filter",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_RES1_WEIGHT:
        length = snprintf(output, 256u, "%s.%llu.convs1.%llu.weight_v",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_RES1_GAIN:
        length = snprintf(output, 256u, "%s.%llu.convs1.%llu.weight_g",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_RES1_BIAS:
        length = snprintf(output, 256u, "%s.%llu.convs1.%llu.bias",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_RES2_WEIGHT:
        length = snprintf(output, 256u, "%s.%llu.convs2.%llu.weight_v",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_RES2_GAIN:
        length = snprintf(output, 256u, "%s.%llu.convs2.%llu.weight_g",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_RES2_BIAS:
        length = snprintf(output, 256u, "%s.%llu.convs2.%llu.bias",
                          templates->residual_blocks, residual, layer); break;
    case YVEX_ALIAS_DECODER_POST_ACT_ALPHA:
        length = snprintf(output, 256u, "%s.act.alpha", templates->post_activation); break;
    case YVEX_ALIAS_DECODER_POST_ACT_BETA:
        length = snprintf(output, 256u, "%s.act.beta", templates->post_activation); break;
    case YVEX_ALIAS_DECODER_POST_ACT_UP_FILTER:
        length = snprintf(output, 256u, "%s.upsample.filter", templates->post_activation); break;
    case YVEX_ALIAS_DECODER_POST_ACT_DOWN_FILTER:
        length = snprintf(output, 256u, "%s.downsample.lowpass.filter",
                          templates->post_activation); break;
    case YVEX_ALIAS_DECODER_POST_WEIGHT:
        length = snprintf(output, 256u, "%s.weight_v", templates->post_convolution); break;
    case YVEX_ALIAS_DECODER_POST_GAIN:
        length = snprintf(output, 256u, "%s.weight_g", templates->post_convolution); break;
    default: break;
    }
    if (length < 0 || length >= 256)
        return signal_program_refuse(err, "alias decoder weight name exceeded its identity bound");
    return YVEX_OK;
}

static yvex_ir_id signal_type(signal_builder *b, unsigned int rank, const unsigned long long *dims, int batch)
{
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = rank};
    yvex_ir_id id = YVEX_IR_NONE;
    for (size_t i = 0u; i < rank; ++i) t.shape[i] = (yvex_ir_extent){batch && !i ? 0u : YVEX_IR_NONE,
        batch && !i ? 0u : dims[i]};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->err);
    return id;
}

static yvex_ir_id signal_op(signal_builder *b, const char *name, const yvex_ir_id *operands, size_t count,
    yvex_ir_id type, const yvex_ir_attribute *attrs, size_t attr_count)
{
    yvex_ir_id op = YVEX_IR_NONE;
    yvex_ir_operation_request r = {.operation = name, .operands = operands, .operand_count = count,
        .result_types = &type, .result_count = 1u, .attributes = attrs, .attribute_count = attr_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_operation_at(b->module, op)->results[0] : YVEX_IR_NONE;
}

static yvex_ir_id signal_parameter(signal_builder *b, yvex_alias_decoder_weight_role role,
    unsigned long long stage, unsigned long long block, unsigned long long layer,
    unsigned int rank, const unsigned long long *dims)
{
    size_t index = b->program->parameter_count;
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    if (index >= b->capacity) {
        b->rc = signal_program_refuse(b->err, "parameter count exceeded source recipe");
        return YVEX_IR_NONE;
    }
    b->rc = b->name(b->name_context, role, stage, block, layer, b->program->parameter_names[index], b->err);
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "weight_%zu", index);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->identity);
    yvex_ir_id type = signal_type(b, rank, dims, 0);
    yvex_ir_id value = signal_op(b, "core.parameter", NULL, 0u, type, attrs, 2u);
    if (b->rc == YVEX_OK) {
        b->bindings[index] = (yvex_program_parameter_binding){value, index, YVEX_GGUF_QTYPE_F32};
        b->program->parameter_count++;
    }
    return value;
}

static signal_value signal_convolution(signal_builder *b, signal_value x,
    unsigned long long channels, unsigned long long kernel, unsigned long long stride,
    unsigned long long dilation, unsigned long long padding, int transposed, int normalized, int biased,
    yvex_alias_decoder_weight_role weight, yvex_alias_decoder_weight_role gain,
    yvex_alias_decoder_weight_role bias, unsigned long long stage, unsigned long long block,
    unsigned long long layer)
{
    signal_value y = {YVEX_IR_NONE, channels, 0u};
    yvex_convolution_1d_geometry geometry = {b->batch, x.channels, channels, x.length,
        kernel, stride, dilation, padding, 0u, transposed};
    if (b->rc == YVEX_OK) b->rc = yvex_convolution_1d_output_length(&geometry, &y.length, b->err);
    unsigned long long dims[] = {transposed ? x.channels : channels, transposed ? channels : x.channels, kernel};
    yvex_ir_id args[4] = {x.value, signal_parameter(b, weight, stage, block, layer, 3u, dims)};
    size_t count = 2u;
    if (normalized) args[count++] = signal_parameter(b, gain, stage, block, layer, 1u, dims);
    if (biased) args[count++] = signal_parameter(b, bias, stage, block, layer, 1u, &channels);
    yvex_ir_attribute attrs[] = {
        {.name = "stride", .kind = YVEX_IR_ATTR_U64, .value.integer = stride},
        {.name = "dilation", .kind = YVEX_IR_ATTR_U64, .value.integer = dilation},
        {.name = "padding", .kind = YVEX_IR_ATTR_U64, .value.integer = padding},
        {.name = "output_padding", .kind = YVEX_IR_ATTR_U64},
        {.name = "transposed", .kind = YVEX_IR_ATTR_BOOL, .value.integer = (unsigned int)transposed}};
    yvex_ir_id type = signal_type(b, 3u, (unsigned long long[]){b->batch, channels, y.length}, 1);
    y.value = signal_op(b, !normalized ? "signal.conv1d" : biased ? "signal.normalized_conv1d" :
        "signal.normalized_conv1d_unbiased", args, count, type, attrs, 5u);
    return y;
}

static signal_value signal_activation(signal_builder *b, signal_value x,
    unsigned long long stage, unsigned long long block, unsigned long long layer, int final)
{
    yvex_ir_id args[5] = {x.value};
    const yvex_alias_decoder_weight_role roles[] = {
        final ? YVEX_ALIAS_DECODER_POST_ACT_ALPHA : YVEX_ALIAS_DECODER_ACT_ALPHA,
        final ? YVEX_ALIAS_DECODER_POST_ACT_BETA : YVEX_ALIAS_DECODER_ACT_BETA,
        final ? YVEX_ALIAS_DECODER_POST_ACT_UP_FILTER : YVEX_ALIAS_DECODER_ACT_UP_FILTER,
        final ? YVEX_ALIAS_DECODER_POST_ACT_DOWN_FILTER : YVEX_ALIAS_DECODER_ACT_DOWN_FILTER};
    for (size_t i = 0u; i < 4u; ++i) {
        unsigned long long width = i < 2u ? x.channels : 12u;
        args[i + 1u] = signal_parameter(b, roles[i], stage, block, layer, 1u, &width);
    }
    yvex_ir_id type = signal_type(b, 3u, (unsigned long long[]){b->batch, x.channels, x.length}, 1);
    x.value = signal_op(b, "signal.alias_snake", args, 5u, type, NULL, 0u);
    return x;
}

static signal_value signal_residual(signal_builder *b, signal_value x,
    unsigned long long stage, unsigned long long block)
{
    const yvex_alias_decoder_recipe *r = b->recipe;
    unsigned long long kernel = r->residual_kernels[block];
    for (size_t layer = 0u; b->rc == YVEX_OK && layer < r->residual_layers; ++layer) {
        signal_value y = signal_activation(b, x, stage, block, layer * 2u, 0);
        unsigned long long dilation = r->residual_dilations[layer];
        y = signal_convolution(b, y, x.channels, kernel, 1u, dilation, (kernel - 1u) * dilation / 2u,
            0, 1, 1, YVEX_ALIAS_DECODER_RES1_WEIGHT, YVEX_ALIAS_DECODER_RES1_GAIN,
            YVEX_ALIAS_DECODER_RES1_BIAS, stage, block, layer);
        y = signal_activation(b, y, stage, block, layer * 2u + 1u, 0);
        y = signal_convolution(b, y, x.channels, kernel, 1u, 1u, (kernel - 1u) / 2u,
            0, 1, 1, YVEX_ALIAS_DECODER_RES2_WEIGHT, YVEX_ALIAS_DECODER_RES2_GAIN,
            YVEX_ALIAS_DECODER_RES2_BIAS, stage, block, layer);
        yvex_ir_id type = signal_type(b, 3u, (unsigned long long[]){b->batch, x.channels, x.length}, 1);
        x.value = signal_op(b, "tensor.add", (yvex_ir_id[]){x.value, y.value}, 2u, type, NULL, 0u);
    }
    return x;
}

static signal_value signal_stages(signal_builder *b, signal_value x)
{
    const yvex_alias_decoder_recipe *r = b->recipe;
    for (size_t stage = 0u; b->rc == YVEX_OK && stage < r->stage_count; ++stage) {
        x = signal_convolution(b, x, x.channels / 2u, r->upsample_kernels[stage], r->rates[stage], 1u,
            (r->upsample_kernels[stage] - r->rates[stage]) / 2u, 1, 1, 1,
            YVEX_ALIAS_DECODER_UP_WEIGHT, YVEX_ALIAS_DECODER_UP_GAIN, YVEX_ALIAS_DECODER_UP_BIAS, stage, 0u, 0u);
        yvex_ir_id branches[YVEX_ALIAS_DECODER_MAX_RESBLOCKS];
        for (size_t block = 0u; b->rc == YVEX_OK && block < r->residual_blocks; ++block)
            branches[block] = signal_residual(b, x, stage, block).value;
        yvex_ir_id type = signal_type(b, 3u, (unsigned long long[]){b->batch, x.channels, x.length}, 1);
        x.value = signal_op(b, "tensor.mean", branches, r->residual_blocks, type, NULL, 0u);
    }
    return x;
}

static int signal_recipe_valid(const yvex_alias_decoder_recipe *r)
{
    if (!r || !r->input_channels || !r->projection_channels || r->input_kernel != 1u ||
        !r->pre_channels || !r->pre_kernel || !(r->pre_kernel & 1u) ||
        !r->stage_count || r->stage_count > YVEX_ALIAS_DECODER_MAX_STAGES ||
        !r->residual_blocks || r->residual_blocks > YVEX_ALIAS_DECODER_MAX_RESBLOCKS ||
        !r->residual_layers || r->residual_layers > YVEX_ALIAS_DECODER_MAX_LAYERS ||
        !r->final_channels || !r->final_kernel || !(r->final_kernel & 1u)) return 0;
    unsigned long long channels = r->pre_channels;
    for (size_t stage = 0u; stage < r->stage_count; ++stage) {
        if (channels < 2u || channels % 2u || !r->rates[stage] ||
            r->upsample_kernels[stage] < r->rates[stage] ||
            (r->upsample_kernels[stage] - r->rates[stage]) % 2u) return 0;
        channels /= 2u;
    }
    for (size_t block = 0u; block < r->residual_blocks; ++block) {
        unsigned long long kernel = r->residual_kernels[block];
        if (!kernel || !(kernel & 1u)) return 0;
        for (size_t layer = 0u; layer < r->residual_layers; ++layer)
            if (!r->residual_dilations[layer] ||
                (kernel - 1u && r->residual_dilations[layer] > UINT64_MAX / (kernel - 1u))) return 0;
    }
    return 1;
}

static int signal_lower(signal_builder *b)
{
    yvex_ir_module *canonical = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    int rc = b->rc;
    if (rc == YVEX_OK) rc = yvex_ir_pass_pipeline(b->module, passes, 2u, &canonical, NULL, b->err);
    size_t used = 0u;
    for (size_t i = 0u; rc == YVEX_OK && i < yvex_ir_operation_count(canonical); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(canonical, i);
        if (strcmp(op->definition->name, "core.parameter")) continue;
        const yvex_ir_attribute *symbol = yvex_ir_attribute_get(canonical, i, "parameter");
        unsigned long long ordinal;
        char trailing;
        if (used >= b->capacity || !symbol ||
            sscanf(symbol->value.text, "weight_%llu%c", &ordinal, &trailing) != 1 ||
            ordinal >= b->program->parameter_count) {
            rc = signal_program_refuse(b->err, "canonical parameter lost source linkage");
            break;
        }
        b->bindings[used++] = (yvex_program_parameter_binding){op->results[0], ordinal, YVEX_GGUF_QTYPE_F32};
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, canonical, b->err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&b->program->physical, execution, b->entrypoint,
        b->bindings, used, yvex_ir_identity(canonical), b->err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&canonical);
    return rc;
}

int yvex_signal_program_compile(yvex_signal_program *out, const yvex_alias_decoder_recipe *r,
    const char *identity, unsigned long long batch, unsigned long long length,
    yvex_alias_decoder_weight_name_fn name, void *name_context, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_id dimension, function, input_type;
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !batch || !length || !name || !yvex_sha256_hex_valid(identity) || !signal_recipe_valid(r))
        return signal_program_refuse(err, "complete bounded source signal recipe and identity required");
    signal_builder b = {.program = out, .recipe = r, .identity = identity, .name = name, .entrypoint = "decode",
        .name_context = name_context, .batch = batch, .err = err};
    b.capacity = (size_t)(11u + r->stage_count * (3u + 14u * r->residual_blocks * r->residual_layers));
    b.bindings = calloc(b.capacity, sizeof(*b.bindings));
    out->parameter_names = calloc(b.capacity, sizeof(*out->parameter_names));
    if (!b.bindings || !out->parameter_names) {
        b.rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "compiler.signal", "parameter linkage allocation failed");
    }
    if (b.rc == YVEX_OK) b.rc = yvex_ir_module_open(&b.module, "signal", identity, dialects, 2u, err);
    yvex_ir_dimension d = {.name = "batch", .minimum = batch, .maximum = batch, .multiple = 1u};
    if (b.rc == YVEX_OK) b.rc = yvex_ir_dimension_add(b.module, &d, &dimension, err);
    input_type = signal_type(&b, 3u, (unsigned long long[]){batch, r->input_channels, length}, 1);
    unsigned long long output_length = length;
    for (size_t i = 0u; b.rc == YVEX_OK && i < r->stage_count; ++i)
        if (!yvex_core_u64_mul(output_length, r->rates[i], &output_length))
            b.rc = signal_program_refuse(err, "signal output length overflowed");
    yvex_ir_id output_type = signal_type(&b, 3u,
        (unsigned long long[]){batch, r->final_channels, output_length}, 1);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_function_add(b.module, "decode", &input_type, 1u,
        &output_type, 1u, 0u, &function, err);
    signal_value x = {YVEX_IR_NONE, r->input_channels, length};
    if (b.rc == YVEX_OK) {
        b.block = yvex_ir_function_at(b.module, function)->body;
        x.value = yvex_ir_block_at(b.module, b.block)->arguments[0];
    }
    x = signal_convolution(&b, x, r->projection_channels, r->input_kernel, 1u, 1u, 0u, 0, 0, 1,
        YVEX_ALIAS_DECODER_INPUT_WEIGHT, YVEX_ALIAS_DECODER_INPUT_WEIGHT, YVEX_ALIAS_DECODER_INPUT_BIAS, 0u, 0u, 0u);
    x = signal_convolution(&b, x, r->pre_channels, r->pre_kernel, 1u, 1u, (r->pre_kernel - 1u) / 2u, 0, 1, 1,
        YVEX_ALIAS_DECODER_PRE_WEIGHT, YVEX_ALIAS_DECODER_PRE_GAIN, YVEX_ALIAS_DECODER_PRE_BIAS, 0u, 0u, 0u);
    x = signal_stages(&b, x);
    x = signal_activation(&b, x, 0u, 0u, 0u, 1);
    x = signal_convolution(&b, x, r->final_channels, r->final_kernel, 1u, 1u, (r->final_kernel - 1u) / 2u, 0, 1, 0,
        YVEX_ALIAS_DECODER_POST_WEIGHT, YVEX_ALIAS_DECODER_POST_GAIN, YVEX_ALIAS_DECODER_POST_GAIN, 0u, 0u, 0u);
    yvex_ir_attribute clamp[] = {{.name = "lower", .kind = YVEX_IR_ATTR_F64, .value.real = -1.0},
        {.name = "upper", .kind = YVEX_IR_ATTR_F64, .value.real = 1.0}};
    x.value = signal_op(&b, "tensor.clamp", &x.value, 1u, output_type, clamp, 2u);
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = &x.value, .operand_count = 1u};
    yvex_ir_id op;
    if (b.rc == YVEX_OK) b.rc = yvex_ir_operation_add(b.module, b.block, &ret, &op, err);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_seal(b.module, err);
    if (b.rc == YVEX_OK) b.rc = signal_lower(&b);
    if (b.rc == YVEX_OK && (!yvex_core_u64_mul(batch, r->final_channels, &out->output_values) ||
        !yvex_core_u64_mul(out->output_values, output_length, &out->output_values)))
        b.rc = signal_program_refuse(err, "signal publication extent overflowed");
    out->output_length = output_length;
    free(b.bindings); yvex_ir_module_close(&b.module);
    if (b.rc != YVEX_OK) yvex_signal_program_close(out);
    return b.rc;
}

typedef struct { yvex_ir_id value; unsigned long long channels, height, width; } spatial_value;

static int spatial_extent(unsigned long long input, unsigned long long kernel, int down,
    unsigned long long *output)
{
    unsigned long long padded;
    if (!input || !kernel || !(kernel & 1u) ||
        !yvex_core_u64_add(input, down ? kernel / 2u : kernel - 1u, &padded) || padded < kernel)
        return 0;
    *output = (padded - kernel) / (down ? 2u : 1u) + 1u;
    return 1;
}

static yvex_ir_id spatial_parameter(signal_builder *b, const yvex_spatial_encoder_recipe *r,
    yvex_spatial_parameter_role role, int bias, unsigned long long stage, unsigned long long block,
    unsigned int rank, const unsigned long long *dims)
{
    size_t index = b->program->parameter_count;
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    if (index >= b->capacity) {
        b->rc = signal_program_refuse(b->err, "spatial parameter count exceeds source topology");
        return YVEX_IR_NONE;
    }
    b->rc = r->parameter_name(r->parameter_context, role, bias, stage, block,
        b->program->parameter_names[index], b->err);
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "weight_%zu", index);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->identity);
    yvex_ir_id type = signal_type(b, rank, dims, 0);
    yvex_ir_id value = signal_op(b, "core.parameter", NULL, 0u, type, attrs, 2u);
    if (b->rc == YVEX_OK) b->program->parameter_count++;
    return value;
}

static yvex_ir_id spatial_type(signal_builder *b, spatial_value x)
{
    return signal_type(b, 4u, (unsigned long long[]){b->batch, x.channels, x.height, x.width}, 1);
}

static spatial_value spatial_conv(signal_builder *b, const yvex_spatial_encoder_recipe *r,
    spatial_value x, unsigned long long channels, unsigned long long kernel, int down,
    yvex_spatial_parameter_role role, unsigned long long stage, unsigned long long block)
{
    spatial_value y = {.value = YVEX_IR_NONE, .channels = channels};
    if (b->rc != YVEX_OK) return y;
    if (!spatial_extent(x.height, kernel, down, &y.height) ||
        !spatial_extent(x.width, kernel, down, &y.width)) {
        b->rc = signal_program_refuse(b->err, "spatial convolution output extent is invalid");
        return y;
    }
    yvex_ir_id args[3] = {x.value};
    args[1] = spatial_parameter(b, r, role, 0, stage, block, 5u,
        (unsigned long long[]){channels, x.channels, kernel, kernel, kernel});
    args[2] = spatial_parameter(b, r, role, 1, stage, block, 1u, &channels);
    yvex_ir_attribute attrs[] = {
        {.name = "stride_height", .kind = YVEX_IR_ATTR_U64, .value.integer = down ? 2u : 1u},
        {.name = "stride_width", .kind = YVEX_IR_ATTR_U64, .value.integer = down ? 2u : 1u},
        {.name = "padding_top", .kind = YVEX_IR_ATTR_U64, .value.integer = down ? 0u : kernel / 2u},
        {.name = "padding_bottom", .kind = YVEX_IR_ATTR_U64, .value.integer = kernel / 2u},
        {.name = "padding_left", .kind = YVEX_IR_ATTR_U64, .value.integer = down ? 0u : kernel / 2u},
        {.name = "padding_right", .kind = YVEX_IR_ATTR_U64, .value.integer = kernel / 2u},
        {.name = "kernel_plane", .kind = YVEX_IR_ATTR_U64, .value.integer = kernel - 1u},
        {.name = "reflect", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 1u}};
    y.value = signal_op(b, "signal.conv2d_slice", args, 3u, spatial_type(b, y), attrs, 8u);
    return y;
}

static spatial_value spatial_norm(signal_builder *b, const yvex_spatial_encoder_recipe *r,
    spatial_value x, yvex_spatial_parameter_role role, unsigned long long stage, unsigned long long block)
{
    yvex_ir_id args[3] = {x.value};
    args[1] = spatial_parameter(b, r, role, 0, stage, block, 1u, &x.channels);
    args[2] = spatial_parameter(b, r, role, 1, stage, block, 1u, &x.channels);
    yvex_ir_attribute attrs[] = {{.name = "groups", .kind = YVEX_IR_ATTR_U64, .value.integer = r->groups},
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = r->epsilon}};
    x.value = signal_op(b, "nn.spatial_group_norm_silu", args, 3u, spatial_type(b, x), attrs, 2u);
    return x;
}

static spatial_value spatial_stages(signal_builder *b, const yvex_spatial_encoder_recipe *r, spatial_value x)
{
    for (unsigned long long stage = 0u; b->rc == YVEX_OK && stage < r->stage_count; ++stage) {
        for (unsigned long long block = 0u; b->rc == YVEX_OK && block < r->stages[stage].blocks; ++block) {
            spatial_value branch = spatial_norm(b, r, x, YVEX_SPATIAL_NORM1, stage, block);
            branch = spatial_conv(b, r, branch, r->stages[stage].channels, r->kernel_size, 0,
                YVEX_SPATIAL_CONV1, stage, block);
            branch = spatial_norm(b, r, branch, YVEX_SPATIAL_NORM2, stage, block);
            branch = spatial_conv(b, r, branch, r->stages[stage].channels, r->kernel_size, 0,
                YVEX_SPATIAL_CONV2, stage, block);
            if (x.channels != branch.channels)
                x = spatial_conv(b, r, x, branch.channels, 1u, 0, YVEX_SPATIAL_SHORTCUT, stage, block);
            yvex_ir_id args[] = {x.value, branch.value};
            x.value = signal_op(b, "tensor.add", args, 2u, spatial_type(b, x), NULL, 0u);
        }
        if (r->stages[stage].downsample)
            x = spatial_conv(b, r, x, x.channels, r->kernel_size, 1, YVEX_SPATIAL_DOWNSAMPLE, stage, 0u);
    }
    return x;
}

int yvex_spatial_encoder_compile(yvex_signal_program *out, const yvex_spatial_encoder_recipe *r,
    const char *identity, unsigned long long batch, unsigned long long height,
    unsigned long long width, yvex_error *err)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !r || !r->parameter_name || !yvex_sha256_hex_valid(identity) || !batch || !height || !width ||
        !r->input_channels || !r->entry_channels || !r->output_channels || !r->projected_channels ||
        !r->groups || !r->stage_count || r->stage_count > 8u || !r->kernel_size || !(r->kernel_size & 1u) ||
        !(r->epsilon > 0.0)) return signal_program_refuse(err, "complete source spatial encoder recipe required");
    signal_builder b = {.program = out, .identity = identity, .entrypoint = "encode", .batch = batch, .err = err,
        .capacity = 16u};
    spatial_value result = {.channels = r->projected_channels, .height = height, .width = width};
    for (size_t i = 0u; i < r->stage_count; ++i) {
        if (!r->stages[i].channels || !r->stages[i].blocks || r->stages[i].blocks > 16u ||
            (r->stages[i].downsample != 0 && r->stages[i].downsample != 1))
            return signal_program_refuse(err, "spatial encoder stage is not bounded");
        b.capacity += (size_t)(10u * r->stages[i].blocks + 2u);
        if (r->stages[i].downsample && (!spatial_extent(result.height, r->kernel_size, 1, &result.height) ||
            !spatial_extent(result.width, r->kernel_size, 1, &result.width)))
            return signal_program_refuse(err, "spatial encoder output is empty or overflowed");
    }
    b.bindings = calloc(b.capacity, sizeof(*b.bindings));
    out->parameter_names = calloc(b.capacity, sizeof(*out->parameter_names));
    if (!b.bindings || !out->parameter_names) {
        b.rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, b.rc, "compiler.signal", "spatial parameter allocation failed");
    }
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_id dimension, function, op;
    if (b.rc == YVEX_OK) b.rc = yvex_ir_module_open(&b.module, "spatial_encoder", identity, dialects, 2u, err);
    yvex_ir_dimension d = {.name = "batch", .minimum = batch, .maximum = batch, .multiple = 1u};
    if (b.rc == YVEX_OK) b.rc = yvex_ir_dimension_add(b.module, &d, &dimension, err);
    spatial_value x = {.value = YVEX_IR_NONE, .channels = r->input_channels, .height = height, .width = width};
    yvex_ir_id input_type = spatial_type(&b, x), output_type = spatial_type(&b, result);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_function_add(b.module, "encode", &input_type, 1u,
        &output_type, 1u, 0u, &function, err);
    if (b.rc == YVEX_OK) {
        b.block = yvex_ir_function_at(b.module, function)->body;
        x.value = yvex_ir_block_at(b.module, b.block)->arguments[0];
    }
    x = spatial_conv(&b, r, x, r->entry_channels, r->kernel_size, 0, YVEX_SPATIAL_INPUT, 0u, 0u);
    x = spatial_stages(&b, r, x);
    x = spatial_norm(&b, r, x, YVEX_SPATIAL_FINAL_NORM, 0u, 0u);
    x = spatial_conv(&b, r, x, r->output_channels, r->kernel_size, 0, YVEX_SPATIAL_FINAL_CONV, 0u, 0u);
    x = spatial_conv(&b, r, x, r->projected_channels, 1u, 0, YVEX_SPATIAL_OUTPUT, 0u, 0u);
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = &x.value, .operand_count = 1u};
    if (b.rc == YVEX_OK) b.rc = yvex_ir_operation_add(b.module, b.block, &ret, &op, err);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_seal(b.module, err);
    if (b.rc == YVEX_OK) b.rc = signal_lower(&b);
    if (b.rc == YVEX_OK && (!yvex_core_u64_mul(result.height, result.width, &out->output_length) ||
        !yvex_core_u64_mul(batch, r->projected_channels, &out->output_values) ||
        !yvex_core_u64_mul(out->output_values, out->output_length, &out->output_values)))
        b.rc = signal_program_refuse(err, "spatial publication extent overflowed");
    yvex_ir_module_close(&b.module);
    free(b.bindings);
    if (b.rc != YVEX_OK) yvex_signal_program_close(out);
    return b.rc;
}

int yvex_signal_program_parameter_name(void *context, unsigned long long index, char output[256], yvex_error *err)
{
    const yvex_signal_program *p = context;
    if (!p || !p->physical || !output || index >= p->parameter_count)
        return signal_program_refuse(err, "parameter ordinal is outside compiled signal linkage");
    yvex_core_text_copy(output, 256u, p->parameter_names[index]);
    return YVEX_OK;
}

void yvex_signal_program_close(yvex_signal_program *p)
{
    if (!p) return;
    yvex_program_physical_close(&p->physical);
    free(p->parameter_names); memset(p, 0, sizeof(*p));
}
