/* Execute the released MiniMax-H3 FL2VA multimodal conditioner on generic CUDA owners. */
#include <yvex/internal/artifact.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/component.h>
#include <yvex/internal/families/minimax_h3.h>
#include <yvex/internal/image.h>
#include <yvex/internal/multimodal.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/tokenizer.h>
#include <yvex/tokenizer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    H3_VISION_START_TOKEN = 151652u,
    H3_VISION_END_TOKEN = 151653u,
    H3_IMAGE_PAD_TOKEN = 151655u,
    H3_VISION_PATCH = 16u,
    H3_VISION_MERGE = 2u,
    H3_CONDITION_WIDTH = 5120u
};

typedef struct {
    unsigned int *ids, *tags, *types;
    unsigned long long count, capacity;
} h3_presentation;

typedef struct {
    yvex_image canvas[YVEX_MEDIA_CONDITION_CAP];
    yvex_image vision[YVEX_MEDIA_CONDITION_CAP];
    unsigned long long source_indices[YVEX_MEDIA_CONDITION_CAP];
    unsigned long long count, grid_height, grid_width, patch_rows, merged_rows;
    float *patches, *merged, *deepstack;
} h3_images;

typedef struct {
    uint32_t state[624];
    unsigned int next;
    int left;
} h3_torch_rng;

typedef struct {
    const yvex_component_execution *component;
    const yvex_component_program_request *program;
    yvex_backend *backend;
    unsigned long long batch, channels, height, width;
    unsigned long long kernel_launches, h2d_bytes, d2h_bytes;
    unsigned long long peak_device_bytes;
} h3_encoder_run;


static int h3_refuse(yvex_error *err, yvex_status status, const char *where,
                     const char *message)
{
    yvex_error_set(err, status, where, message);
    return status;
}


static unsigned long long h3_round_even_div(unsigned long long value,
                                             unsigned long long divisor)
{
    unsigned long long quotient = value / divisor, remainder = value % divisor;
    if (remainder * 2ull > divisor ||
        (remainder * 2ull == divisor && quotient % 2ull)) ++quotient;
    return quotient;
}

static int h3_image_copy(const yvex_image *source, yvex_image *output, yvex_error *err)
{
    if (!source || !source->complete || !source->data || !output ||
        source->data_bytes > SIZE_MAX)
        return h3_refuse(err, YVEX_ERR_INVALID_ARG, "minimax-h3.image.copy",
                         "one complete bounded RGB image is required");
    memset(output, 0, sizeof(*output));
    output->data = malloc((size_t)source->data_bytes);
    if (!output->data)
        return h3_refuse(err, YVEX_ERR_NOMEM, "minimax-h3.image.copy",
                         "image copy allocation failed");
    memcpy(output->data, source->data, (size_t)source->data_bytes);
    *output = (yvex_image){
        .schema_version = source->schema_version, .format = source->format,
        .width = source->width, .height = source->height, .channels = source->channels,
        .row_bytes = source->row_bytes, .data_bytes = source->data_bytes,
        .source_file_bytes = source->source_file_bytes, .data = output->data, .complete = 1,
    };
    memcpy(output->source_identity, source->source_identity, sizeof(output->source_identity));
    memcpy(output->content_identity, source->content_identity, sizeof(output->content_identity));
    return YVEX_OK;
}

static int h3_canvas_prepare(const yvex_image *source, unsigned long long packed_index,
                             unsigned long long width, unsigned long long height,
                             yvex_image *output, yvex_error *err)
{
    yvex_image_resize_request resize = {
        .schema_version = YVEX_IMAGE_RESIZE_SCHEMA_V1,
        .resized_width = width, .resized_height = height,
        .output_width = width, .output_height = height,
    };
    if (source->width == width && source->height == height)
        return h3_image_copy(source, output, err);
    if (packed_index) {
        double scale = fmax((double)width / (double)source->width,
                            (double)height / (double)source->height);
        resize.resized_width = (unsigned long long)nearbyint((double)source->width * scale);
        resize.resized_height = (unsigned long long)nearbyint((double)source->height * scale);
        if (resize.resized_width < width) resize.resized_width = width;
        if (resize.resized_height < height) resize.resized_height = height;
        resize.crop_left = (resize.resized_width - width) / 2ull;
        resize.crop_top = (resize.resized_height - height) / 2ull;
    }
    return yvex_image_resize_lanczos_rgb8(source, &resize, output, err);
}

static int h3_vision_size(const yvex_image *image, unsigned long long *height,
                          unsigned long long *width, yvex_error *err)
{
    const unsigned long long factor = H3_VISION_PATCH * H3_VISION_MERGE;
    unsigned long long h = h3_round_even_div(image->height, factor) * factor;
    unsigned long long w = h3_round_even_div(image->width, factor) * factor;
    long double pixels;
    if (!h) h = factor;
    if (!w) w = factor;
    pixels = (long double)h * (long double)w;
    if (pixels < 65536.0L) {
        long double beta = sqrtl(65536.0L /
                                 ((long double)image->height * image->width));
        h = (unsigned long long)ceill((long double)image->height * beta / factor) * factor;
        w = (unsigned long long)ceill((long double)image->width * beta / factor) * factor;
    } else if (pixels > 16777216.0L) {
        long double beta = sqrtl(((long double)image->height * image->width) / 16777216.0L);
        h = (unsigned long long)floorl((long double)image->height / beta / factor) * factor;
        w = (unsigned long long)floorl((long double)image->width / beta / factor) * factor;
        if (h < factor) h = factor;
        if (w < factor) w = factor;
    }
    if (!h || !w || h % factor || w % factor ||
        (long double)(h > w ? h : w) / (long double)(h < w ? h : w) > 200.0L)
        return h3_refuse(err, YVEX_ERR_FORMAT, "minimax-h3.processor.geometry",
                         "image cannot satisfy Qwen3-VL processor geometry");
    *height = h; *width = w;
    return YVEX_OK;
}

static int h3_condition_order(const yvex_media_condition *conditions,
                              unsigned long long condition_count,
                              h3_images *images, yvex_error *err)
{
    unsigned long long index;
    int first = -1, last = -1;
    for (index = 0ull; index < condition_count; ++index) {
        if (conditions[index].kind != YVEX_MEDIA_CONDITION_IMAGE)
            return h3_refuse(err, YVEX_ERR_UNSUPPORTED, "minimax-h3.condition.kind",
                             "FL2VA accepts image conditions only");
        if (conditions[index].role == YVEX_MEDIA_CONDITION_FIRST && first < 0)
            first = (int)index;
        else if (conditions[index].role == YVEX_MEDIA_CONDITION_LAST && last < 0)
            last = (int)index;
        else
            return h3_refuse(err, YVEX_ERR_FORMAT, "minimax-h3.condition.role",
                             "FL2VA first and last condition roles must be unique");
    }
    images->count = condition_count;
    if (first >= 0) images->source_indices[0] = (unsigned long long)first;
    if (last >= 0) images->source_indices[first >= 0 ? 1u : 0u] = (unsigned long long)last;
    return images->count && first < 0 && last < 0
               ? h3_refuse(err, YVEX_ERR_FORMAT, "minimax-h3.condition.role",
                            "FL2VA requires a first or last keyframe role")
               : YVEX_OK;
}

static int h3_images_prepare(const yvex_media_conditioning_request *request,
                             h3_images *images, yvex_error *err)
{
    unsigned long long index, vision_height = 0ull, vision_width = 0ull;
    int rc = h3_condition_order(request->conditions, request->condition_count, images, err);
    for (index = 0ull; rc == YVEX_OK && index < images->count; ++index) {
        const yvex_image *source = request->condition_images + images->source_indices[index];
        yvex_image_resize_request resize = {.schema_version = YVEX_IMAGE_RESIZE_SCHEMA_V1};
        rc = h3_canvas_prepare(source, index, request->width, request->height,
                               images->canvas + index, err);
        if (rc == YVEX_OK)
            rc = h3_vision_size(images->canvas + index, &vision_height, &vision_width, err);
        if (rc == YVEX_OK && index &&
            (images->vision[0].height != vision_height ||
             images->vision[0].width != vision_width))
            rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.processor.geometry",
                            "one canvas produced inconsistent Qwen3-VL image grids");
        resize.resized_width = resize.output_width = vision_width;
        resize.resized_height = resize.output_height = vision_height;
        if (rc == YVEX_OK)
            rc = yvex_image_resize_bicubic_rgb8(images->canvas + index, &resize,
                                                 images->vision + index, err);
    }
    if (rc == YVEX_OK && images->count) {
        images->grid_height = images->vision[0].height / H3_VISION_PATCH;
        images->grid_width = images->vision[0].width / H3_VISION_PATCH;
        if (!yvex_core_u64_mul(images->grid_height, images->grid_width,
                               &images->patch_rows) ||
            !yvex_core_u64_mul(images->patch_rows, images->count,
                               &images->patch_rows) ||
            !yvex_core_u64_mul(images->grid_height / H3_VISION_MERGE,
                               images->grid_width / H3_VISION_MERGE,
                               &images->merged_rows) ||
            !yvex_core_u64_mul(images->merged_rows, images->count,
                               &images->merged_rows))
            rc = h3_refuse(err, YVEX_ERR_BOUNDS, "minimax-h3.processor.geometry",
                            "Qwen3-VL image grid overflowed");
    }
    return rc;
}

static int h3_patches_build(h3_images *images, yvex_error *err)
{
    const unsigned long long patch_width = 3ull * 2ull * 16ull * 16ull;
    unsigned long long values, image, block_h, block_w, merge_h, merge_w;
    unsigned long long channel, temporal, patch_h, patch_w, cursor = 0ull;
    if (!yvex_core_u64_mul(images->patch_rows, patch_width, &values) ||
        values > SIZE_MAX / sizeof(float) ||
        !(images->patches = malloc((size_t)values * sizeof(float))))
        return h3_refuse(err, YVEX_ERR_NOMEM, "minimax-h3.processor.patches",
                         "Qwen3-VL patch allocation failed");
    for (image = 0ull; image < images->count; ++image)
        for (block_h = 0ull; block_h < images->grid_height / 2ull; ++block_h)
            for (block_w = 0ull; block_w < images->grid_width / 2ull; ++block_w)
                for (merge_h = 0ull; merge_h < 2ull; ++merge_h)
                    for (merge_w = 0ull; merge_w < 2ull; ++merge_w)
                        for (channel = 0ull; channel < 3ull; ++channel)
                            for (temporal = 0ull; temporal < 2ull; ++temporal)
                                for (patch_h = 0ull; patch_h < 16ull; ++patch_h)
                                    for (patch_w = 0ull; patch_w < 16ull; ++patch_w) {
                                        unsigned long long y =
                                            (block_h * 2ull + merge_h) * 16ull + patch_h;
                                        unsigned long long x =
                                            (block_w * 2ull + merge_w) * 16ull + patch_w;
                                        size_t source = ((size_t)y * images->vision[image].width +
                                                         (size_t)x) * 3u + (size_t)channel;
                                        images->patches[cursor++] =
                                            (float)images->vision[image].data[source] /
                                                127.5f - 1.0f;
                                    }
    if (cursor != values)
        return h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.processor.patches",
                         "Qwen3-VL patch packing did not close");
    return YVEX_OK;
}

static void h3_images_close(h3_images *images)
{
    unsigned long long index;
    if (!images) return;
    for (index = 0ull; index < YVEX_MEDIA_CONDITION_CAP; ++index) {
        yvex_image_close(images->vision + index);
        yvex_image_close(images->canvas + index);
    }
    free(images->deepstack); free(images->merged); free(images->patches);
    memset(images, 0, sizeof(*images));
}

static int h3_presentation_append(h3_presentation *presentation,
                                  const unsigned int *ids, unsigned long long count,
                                  unsigned int tag, unsigned int type, yvex_error *err)
{
    unsigned long long index;
    if (!presentation || (!ids && count) || presentation->count > presentation->capacity ||
        count > presentation->capacity - presentation->count)
        return h3_refuse(err, YVEX_ERR_BOUNDS, "minimax-h3.presentation",
                         "FL2VA presentation exceeds its token capacity");
    for (index = 0ull; index < count; ++index) {
        presentation->ids[presentation->count] = ids[index];
        presentation->tags[presentation->count] = tag;
        presentation->types[presentation->count++] = type;
    }
    return YVEX_OK;
}

static int h3_presentation_text(h3_presentation *presentation, yvex_tokenizer *tokenizer,
                                const char *text, unsigned int tag, yvex_error *err)
{
    yvex_tokenizer_encode_options options = {0, 0, 1, 0};
    yvex_tokenizer_encode_result encoded = {0};
    yvex_tokens fallback = {0};
    const yvex_tokens *tokens;
    int rc;
    options.maximum_tokens = presentation->capacity - presentation->count;
    if (yvex_tokenizer_plan_summary_get(tokenizer)) {
        rc = yvex_tokenizer_encode(tokenizer, (const unsigned char *)text,
                                   (unsigned long long)strlen(text), &options, &encoded, err);
        tokens = &encoded.tokens;
    } else {
        rc = yvex_tokenize_text(tokenizer, text, &fallback, err);
        tokens = &fallback;
    }
    if (rc == YVEX_OK)
        rc = h3_presentation_append(presentation, tokens->ids, tokens->len, tag, 0u, err);
    yvex_tokenizer_encode_result_clear(&encoded);
    yvex_tokens_clear(&fallback);
    return rc;
}

static int h3_presentation_build(const yvex_media_conditioning_request *request,
                                 const h3_images *images, h3_presentation *presentation,
                                 unsigned int **visual_indices,
                                 unsigned long long **position_ids, yvex_error *err)
{
    unsigned long long image, index, visual = 0ull, current = 0ull, grid_index = 0ull;
    unsigned int token;
    int rc = YVEX_OK;
    presentation->capacity = request->maximum_prompt_tokens;
    presentation->ids = calloc((size_t)presentation->capacity, sizeof(*presentation->ids));
    presentation->tags = request->text_tags;
    presentation->types = calloc((size_t)presentation->capacity, sizeof(*presentation->types));
    *visual_indices = images->merged_rows
                          ? calloc((size_t)images->merged_rows, sizeof(**visual_indices)) : NULL;
    *position_ids = calloc((size_t)presentation->capacity * 3u, sizeof(**position_ids));
    if (!presentation->ids || !presentation->types || !*position_ids ||
        (images->merged_rows && !*visual_indices))
        rc = h3_refuse(err, YVEX_ERR_NOMEM, "minimax-h3.presentation",
                        "FL2VA presentation allocation failed");
    for (image = 0ull; rc == YVEX_OK && image < images->count; ++image) {
        char label[32];
        int written = snprintf(label, sizeof(label), "<Picture %llu>: ", image + 1ull);
        if (written < 0 || (size_t)written >= sizeof(label))
            rc = h3_refuse(err, YVEX_ERR_BOUNDS, "minimax-h3.presentation",
                            "FL2VA keyframe label exceeded its bound");
        if (rc == YVEX_OK)
            rc = h3_presentation_text(presentation, request->tokenizer, label, 1u, err);
        token = H3_VISION_START_TOKEN;
        if (rc == YVEX_OK)
            rc = h3_presentation_append(presentation, &token, 1ull, 0u, 0u, err);
        token = H3_IMAGE_PAD_TOKEN;
        for (index = 0ull; rc == YVEX_OK && index <
             (images->grid_height / 2ull) * (images->grid_width / 2ull); ++index) {
            (*visual_indices)[visual++] = (unsigned int)presentation->count;
            rc = h3_presentation_append(presentation, &token, 1ull, 0u, 1u, err);
        }
        token = H3_VISION_END_TOKEN;
        if (rc == YVEX_OK)
            rc = h3_presentation_append(presentation, &token, 1ull, 0u, 0u, err);
    }
    if (rc == YVEX_OK)
        rc = h3_presentation_text(presentation, request->tokenizer, request->prompt, 1u, err);
    for (index = 0ull; rc == YVEX_OK && index < presentation->count;) {
        unsigned long long end = index + 1ull;
        while (end < presentation->count &&
               presentation->types[end] == presentation->types[index]) ++end;
        if (!presentation->types[index]) {
            for (unsigned long long row = index; row < end; ++row)
                for (unsigned long long axis = 0ull; axis < 3ull; ++axis)
                    (*position_ids)[axis * presentation->count + row] = current + row - index;
            current += end - index;
        } else {
            unsigned long long merged_h = images->grid_height / 2ull;
            unsigned long long merged_w = images->grid_width / 2ull;
            if (end - index != merged_h * merged_w || grid_index++ >= images->count)
                rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.presentation.position",
                                "Qwen3-VL media run differs from its processor grid");
            for (unsigned long long row = index; rc == YVEX_OK && row < end; ++row) {
                unsigned long long local = row - index;
                (*position_ids)[row] = current;
                (*position_ids)[presentation->count + row] = current + local / merged_w;
                (*position_ids)[presentation->count * 2ull + row] = current + local % merged_w;
            }
            current += merged_h > merged_w ? merged_h : merged_w;
        }
        index = end;
    }
    if (rc == YVEX_OK && (visual != images->merged_rows || !presentation->count))
        rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.presentation",
                        "FL2VA presentation did not consume every visual token");
    return rc;
}

static int h3_token_identity(const h3_presentation *presentation,
                             char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&hash);
    if (!presentation || !presentation->ids || !presentation->count ||
        !yvex_sha256_update_text(&hash, "yvex.minimax-h3.fl2va.presentation.v1") ||
        !yvex_sha256_update_u64_be(&hash, presentation->count)) return 0;
    for (index = 0ull; index < presentation->count; ++index)
        if (!yvex_sha256_update_u64_be(&hash, presentation->ids[index]) ||
            !yvex_sha256_update_u64_be(&hash, presentation->tags[index])) return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int h3_processor_identity(const h3_presentation *presentation,
                                 const h3_images *images,
                                 char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index, values;
    if (!yvex_core_u64_mul(images->patch_rows, 1536ull, &values)) return 0;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.minimax-h3.qwen3-vl.processor.v1") ||
        !yvex_sha256_update_u64_be(&hash, images->count) ||
        !yvex_sha256_update_u64_be(&hash, images->grid_height) ||
        !yvex_sha256_update_u64_be(&hash, images->grid_width)) return 0;
    for (index = 0ull; index < images->count; ++index)
        if (!yvex_sha256_update_text(&hash, images->canvas[index].content_identity) ||
            !yvex_sha256_update_text(&hash, images->vision[index].content_identity)) return 0;
    for (index = 0ull; index < values; ++index) {
        unsigned int bits;
        memcpy(&bits, images->patches + index, sizeof(bits));
        if (!yvex_sha256_update_u64_be(&hash, bits)) return 0;
    }
    if (!yvex_sha256_update_u64_be(&hash, presentation->count) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int h3_text_only(const yvex_media_conditioning_request *request,
                        yvex_runtime_av_conditioning_result *result, yvex_error *err)
{
    h3_presentation presentation = {0};
    yvex_component_text_request text = {
        .program = request->text_program,
        .parameter_name = request->text_parameter_name,
        .parameter_context = request->text_parameter_context,
    };
    int rc;
    presentation.capacity = request->maximum_prompt_tokens;
    presentation.ids = calloc((size_t)presentation.capacity, sizeof(*presentation.ids));
    presentation.types = calloc((size_t)presentation.capacity, sizeof(*presentation.types));
    presentation.tags = request->text_tags;
    if (!presentation.ids || !presentation.types)
        rc = h3_refuse(err, YVEX_ERR_NOMEM, "minimax-h3.presentation",
                        "T2VA presentation allocation failed");
    else rc = h3_presentation_text(&presentation, request->tokenizer, request->prompt, 1u, err);
    text.token_ids = presentation.ids; text.token_count = presentation.count;
    text.output = request->conditioning;
    text.output_capacity = request->conditioning_capacity;
    if (rc == YVEX_OK)
        rc = yvex_component_text_execute(
            request->text_component, &text, result, err);
    if (rc == YVEX_OK && !h3_token_identity(&presentation, result->prompt_identity))
        rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.presentation.identity",
                        "T2VA presentation identity could not be sealed");
    if (rc == YVEX_OK)
        memcpy(result->processor_identity, result->prompt_identity,
               sizeof(result->processor_identity));
    free(presentation.types); free(presentation.ids);
    return rc;
}

int yvex_backend_minimax_h3_fl2va_condition(
    const yvex_media_conditioning_request *request,
    yvex_runtime_av_conditioning_result *result, yvex_error *err)
{
    h3_images images = {0};
    h3_presentation presentation = {0};
    yvex_component_text_request text = {0};
    yvex_backend_text_multimodal_input multimodal = {0};
    yvex_vision_request vision = {0};
    yvex_vision_result vision_result = {0};
    unsigned int *visual_indices = NULL;
    unsigned long long *position_ids = NULL;
    unsigned long long merged_values = 0u, deep_values = 0u;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!request || request->schema_version != YVEX_MEDIA_CONDITIONING_SCHEMA_V3)
        return h3_refuse(err, YVEX_ERR_INVALID_ARG, "minimax-h3.fl2va.conditioning",
            "current typed conditioning request required");
    if (request->condition_count && !request->vision_entry)
        return h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.vision.entry",
            "image conditioning requires its admitted compiler entry");
    if (!request->prompt || !request->tokenizer || !request->text_component ||
        !request->conditioning || !request->text_tags || !result ||
        request->condition_count > YVEX_MEDIA_CONDITION_CAP ||
        (request->condition_count && (!request->conditions || !request->condition_images)))
        return h3_refuse(err, YVEX_ERR_INVALID_ARG, "minimax-h3.fl2va.conditioning",
                         "one admitted typed FL2VA conditioning request is required");
    if (!request->condition_count) return h3_text_only(request, result, err);
    rc = h3_images_prepare(request, &images, err);
    if (rc == YVEX_OK) rc = h3_patches_build(&images, err);
    if (rc == YVEX_OK &&
        (!yvex_core_u64_mul(images.merged_rows, H3_CONDITION_WIDTH, &merged_values) ||
         !yvex_core_u64_mul(merged_values, 3ull, &deep_values) ||
         merged_values > SIZE_MAX / sizeof(float) || deep_values > SIZE_MAX / sizeof(float) ||
         !(images.merged = malloc((size_t)merged_values * sizeof(float))) ||
         !(images.deepstack = malloc((size_t)deep_values * sizeof(float)))))
        rc = h3_refuse(err, YVEX_ERR_NOMEM, "minimax-h3.vision.output",
                        "Qwen3-VL visual output allocation failed");
    if (rc == YVEX_OK)
        rc = h3_presentation_build(request, &images, &presentation,
                                   &visual_indices, &position_ids, err);
    vision = (yvex_vision_request){
        .patches = images.patches,
        .patch_rows = images.patch_rows, .patch_capacity = images.patch_rows * 1536ull,
        .image_count = images.count, .grid_height = images.grid_height,
        .grid_width = images.grid_width,
        .merged = images.merged, .deepstack = images.deepstack,
        .merged_capacity = merged_values, .deepstack_capacity = deep_values,
        .observe = request->vision_observe,
        .observer_context = request->vision_observer_context,
    };
    if (rc == YVEX_OK)
        rc = request->vision_entry(
            request->text_component, &vision, &vision_result, err);
    if (rc == YVEX_OK && request->observe) {
        yvex_media_conditioning_observation observation = {
            .token_ids = presentation.ids,
            .token_types = presentation.types,
            .text_tags = presentation.tags,
            .position_ids = position_ids,
            .token_count = presentation.count,
            .image_count = images.count,
            .grid_height = images.grid_height,
            .grid_width = images.grid_width,
            .vision_patches = images.patches,
            .patch_values = images.patch_rows * 1536ull,
            .vision_merged = images.merged,
            .vision_deepstack = images.deepstack,
            .merged_values = merged_values,
            .deepstack_values = deep_values,
        };
        rc = request->observe(request->observer_context, &observation, err);
    }
    multimodal.position_ids = position_ids;
    multimodal.position_capacity = presentation.count * 3ull;
    multimodal.visual_token_indices = visual_indices;
    multimodal.visual_token_count = images.merged_rows;
    multimodal.visual_embeddings = images.merged;
    multimodal.visual_embedding_capacity = merged_values;
    multimodal.deepstack_embeddings = images.deepstack;
    multimodal.deepstack_layer_count = 3ull;
    multimodal.deepstack_embedding_capacity = deep_values;
    multimodal.mrope_sections[0] = 24ull;
    multimodal.mrope_sections[1] = multimodal.mrope_sections[2] = 20ull;
    multimodal.vision_execution_identity = vision_result.execution_identity;
    text.program = request->text_program;
    text.parameter_name = request->text_parameter_name;
    text.parameter_context = request->text_parameter_context;
    text.token_ids = presentation.ids; text.token_count = presentation.count;
    text.multimodal = &multimodal;
    text.output = request->conditioning; text.output_capacity = request->conditioning_capacity;
    if (rc == YVEX_OK)
        rc = yvex_component_text_execute(
            request->text_component, &text, result, err);
    if (rc == YVEX_OK &&
        (!h3_token_identity(&presentation, result->prompt_identity) ||
         !h3_processor_identity(&presentation, &images, result->processor_identity)))
        rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.conditioning.identity",
                        "multimodal conditioning identities could not be sealed");
    if (rc == YVEX_OK) {
        result->condition_count = images.count;
        memcpy(result->vision_identity, vision_result.execution_identity,
               sizeof(result->vision_identity));
        for (unsigned long long index = 0ull; index < images.count; ++index)
            memcpy(result->media_identities[index], images.canvas[index].content_identity,
                   sizeof(result->media_identities[index]));
        if (!yvex_core_u64_add(result->kernel_launches, vision_result.kernel_launches,
                               &result->kernel_launches) ||
            !yvex_core_u64_add(result->h2d_bytes, vision_result.h2d_bytes,
                               &result->h2d_bytes) ||
            !yvex_core_u64_add(result->d2h_bytes, vision_result.d2h_bytes,
                               &result->d2h_bytes))
            rc = h3_refuse(err, YVEX_ERR_BOUNDS, "minimax-h3.conditioning.facts",
                            "multimodal conditioning telemetry overflowed");
        if (vision_result.device_bytes > result->device_bytes)
            result->device_bytes = vision_result.device_bytes;
    }
    free(position_ids); free(visual_indices);
    free(presentation.types); free(presentation.ids);
    h3_images_close(&images);
    if (rc != YVEX_OK) memset(result, 0, sizeof(*result));
    return rc;
}

static void h3_rng_seed(h3_torch_rng *rng, unsigned long long seed)
{
    unsigned int index;
    memset(rng, 0, sizeof(*rng));
    rng->state[0] = (uint32_t)seed;
    for (index = 1u; index < 624u; ++index)
        rng->state[index] = 1812433253u *
                                (rng->state[index - 1u] ^
                                 (rng->state[index - 1u] >> 30u)) +
                            index;
    rng->left = 1;
}

static void h3_rng_refresh(h3_torch_rng *rng)
{
    unsigned int index;
    for (index = 0u; index < 227u; ++index) {
        uint32_t mixed = (rng->state[index] & 0x80000000u) |
                         (rng->state[index + 1u] & 0x7fffffffu);
        uint32_t twisted = (mixed >> 1u) ^
                           ((mixed & 1u) ? 0x9908b0dfu : 0u);
        rng->state[index] = rng->state[index + 397u] ^ twisted;
    }
    for (; index < 623u; ++index) {
        uint32_t mixed = (rng->state[index] & 0x80000000u) |
                         (rng->state[index + 1u] & 0x7fffffffu);
        uint32_t twisted = (mixed >> 1u) ^
                           ((mixed & 1u) ? 0x9908b0dfu : 0u);
        rng->state[index] = rng->state[index - 227u] ^ twisted;
    }
    {
        uint32_t mixed = (rng->state[623] & 0x80000000u) |
                         (rng->state[0] & 0x7fffffffu);
        uint32_t twisted = (mixed >> 1u) ^
                           ((mixed & 1u) ? 0x9908b0dfu : 0u);
        rng->state[623] = rng->state[396] ^ twisted;
    }
    rng->left = 624;
    rng->next = 0u;
}

static uint32_t h3_rng_next(h3_torch_rng *rng)
{
    uint32_t value;
    if (--rng->left == 0) h3_rng_refresh(rng);
    value = rng->state[rng->next++];
    value ^= value >> 11u;
    value ^= (value << 7u) & 0x9d2c5680u;
    value ^= (value << 15u) & 0xefc60000u;
    value ^= value >> 18u;
    return value;
}

static void h3_torch_normals(float *output, unsigned long long count,
                             unsigned long long seed)
{
    h3_torch_rng rng;
    unsigned long long index, block, lane;
    h3_rng_seed(&rng, seed);
    for (index = 0ull; index < count; ++index)
        output[index] = (float)(h3_rng_next(&rng) & 0x00ffffffu) / 16777216.0f;
    for (block = 0ull; block + 15ull < count; block += 16ull) {
        float uniforms[16];
        memcpy(uniforms, output + block, sizeof(uniforms));
        for (lane = 0ull; lane < 8ull; ++lane) {
            float radius = sqrtf(-2.0f * logf(1.0f - uniforms[lane]));
            float angle = 6.28318530717958647692f * uniforms[8ull + lane];
            output[block + lane] = radius * cosf(angle);
            output[block + 8ull + lane] = radius * sinf(angle);
        }
    }
    if (count % 16ull) {
        unsigned long long start = count - 16ull;
        float uniforms[16];
        for (index = 0ull; index < 16ull; ++index)
            uniforms[index] =
                (float)(h3_rng_next(&rng) & 0x00ffffffu) / 16777216.0f;
        for (lane = 0ull; lane < 8ull; ++lane) {
            float radius = sqrtf(-2.0f * logf(1.0f - uniforms[lane]));
            float angle = 6.28318530717958647692f * uniforms[8ull + lane];
            output[start + lane] = radius * cosf(angle);
            output[start + 8ull + lane] = radius * sinf(angle);
        }
    }
}

static int h3_encoder_execute(h3_encoder_run *run, const float *pixels,
    float *moments, unsigned long long moment_count, yvex_error *err)
{
    yvex_component_program_result result = {0};
    unsigned long long input_count;
    if (!yvex_core_u64_mul(run->batch, 3u, &input_count) ||
        !yvex_core_u64_mul(input_count, run->height, &input_count) ||
        !yvex_core_u64_mul(input_count, run->width, &input_count))
        return h3_refuse(err, YVEX_ERR_BOUNDS, "minimax-h3.vae.input", "input extent overflowed");
    const float *inputs[] = {pixels};
    float *outputs[] = {moments};
    yvex_component_program_request request = *run->program;
    request.inputs = inputs; request.input_capacity = &input_count;
    request.outputs = outputs; request.output_capacity = &moment_count;
    int rc = yvex_component_tensor_program_execute(run->component, &request, &result, err);
    if (rc == YVEX_OK) {
        run->kernel_launches = result.facts.kernel_launches;
        run->h2d_bytes = result.facts.h2d_bytes;
        run->d2h_bytes = result.facts.d2h_bytes;
        run->peak_device_bytes = result.device_bytes;
    }
    return rc;
}

static int h3_latent_identity(const float *values, unsigned long long count,
                              char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&hash);
    if (!values || !count ||
        !yvex_sha256_update_text(&hash, "yvex.minimax-h3.keyframe-latent.v1") ||
        !yvex_sha256_update_u64_be(&hash, count)) return 0;
    for (index = 0ull; index < count; ++index) {
        uint32_t bits;
        memcpy(&bits, values + index, sizeof(bits));
        if (!yvex_sha256_update_u64_be(&hash, bits)) return 0;
    }
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int h3_keyframe_result_identity(
    const yvex_runtime_av_keyframe_result *result,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.minimax-h3.keyframe-encode.v1") ||
        !yvex_sha256_update_text(&hash, result->residency_identity) ||
        !yvex_sha256_update_u64_be(&hash, result->condition_count) ||
        !yvex_sha256_update_u64_be(&hash, result->latent_height) ||
        !yvex_sha256_update_u64_be(&hash, result->latent_width)) return 0;
    for (index = 0ull; index < result->condition_count; ++index)
        if (!yvex_sha256_update_text(&hash, result->media_identities[index]) ||
            !yvex_sha256_update_text(&hash, result->latent_identities[index])) return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

int yvex_backend_minimax_h3_keyframe_execute(
    const yvex_media_keyframe_request *request,
    const yvex_component_program_request *program,
    yvex_runtime_av_keyframe_result *result, yvex_error *err)
{
    h3_images images = {0};
    h3_encoder_run run = {0};
    float *pixels = NULL, *moments = NULL, *noise = NULL;
    unsigned long long latent_height, latent_width, spatial, latent_values;
    unsigned long long pixel_values, moment_values, image, channel, position;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!request || request->schema_version != YVEX_MEDIA_CONDITIONING_SCHEMA_V3 ||
        !request->conditions || !request->condition_images ||
        !request->condition_count || request->condition_count > YVEX_MEDIA_CONDITION_CAP ||
        !request->width || !request->height || request->width % 16ull ||
        request->height % 16ull || request->pixel_channels != 3ull ||
        request->latent_channels != 24ull || !request->pixel_mean ||
        !request->pixel_std || !request->latent_mean || !request->latent_std ||
        !request->video_component || !request->condition_latents || !program || !result)
        return h3_refuse(err, YVEX_ERR_INVALID_ARG, "minimax-h3.fl2va.keyframe",
                         "one admitted typed FL2VA keyframe request is required");
    if (!program->program || !program->parameter_name || program->rows != request->condition_count ||
        program->input_count != 1u || program->output_count != 1u || program->index_inputs)
        return h3_refuse(err, YVEX_ERR_FORMAT, "minimax-h3.vae.program",
                        "keyframe execution requires one compiled encoder invocation");
    rc = h3_condition_order(request->conditions, request->condition_count, &images, err);
    for (image = 0ull; rc == YVEX_OK && image < images.count; ++image)
        rc = h3_canvas_prepare(request->condition_images + images.source_indices[image],
                               image, request->width, request->height,
                               images.canvas + image, err);
    latent_height = request->height / 16ull;
    latent_width = request->width / 16ull;
    if (!yvex_core_u64_mul(latent_height, latent_width, &spatial) ||
        !yvex_core_u64_mul(request->condition_count * 24ull, spatial, &latent_values) ||
        latent_values != request->condition_latent_capacity ||
        !yvex_core_u64_mul(request->condition_count * 3ull,
                           request->height * request->width, &pixel_values) ||
        !yvex_core_u64_mul(request->condition_count * 48ull, spatial, &moment_values) ||
        pixel_values > SIZE_MAX / sizeof(float) ||
        moment_values > SIZE_MAX / sizeof(float) ||
        spatial * 24ull > SIZE_MAX / sizeof(float))
        rc = h3_refuse(err, YVEX_ERR_BOUNDS, "minimax-h3.vae.geometry",
                        "Visual VAE keyframe geometry overflowed its admitted buffers");
    if (rc == YVEX_OK) {
        pixels = malloc((size_t)pixel_values * sizeof(float));
        moments = malloc((size_t)moment_values * sizeof(float));
        noise = malloc((size_t)(spatial * 24ull) * sizeof(float));
        if (!pixels || !moments || !noise)
            rc = h3_refuse(err, YVEX_ERR_NOMEM, "minimax-h3.vae.workspace",
                            "Visual VAE host workspace allocation failed");
    }
    for (image = 0ull; rc == YVEX_OK && image < images.count; ++image)
        for (channel = 0ull; channel < 3ull; ++channel)
            for (position = 0ull; position < request->height * request->width; ++position) {
                size_t source = (size_t)position * 3u + (size_t)channel;
                size_t destination = ((size_t)image * 3u + (size_t)channel) *
                                     (size_t)(request->height * request->width) +
                                     (size_t)position;
                pixels[destination] =
                    ((float)images.canvas[image].data[source] / 255.0f -
                     request->pixel_mean[channel]) / request->pixel_std[channel];
            }
    run.component = request->video_component;
    run.program = program;
    run.backend = request->video_component->backend;
    run.batch = request->condition_count;
    run.channels = 3ull;
    run.height = request->height;
    run.width = request->width;
    if (rc == YVEX_OK && !run.backend)
        rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.vae.execution",
                        "Visual VAE component has no admitted CUDA backend");
    if (rc == YVEX_OK) rc = h3_encoder_execute(&run, pixels, moments, moment_values, err);
    if (rc == YVEX_OK && request->observe)
        rc = request->observe(request->observer_context, moments, moment_values,
                              request->condition_count, latent_height, latent_width, err);
    for (image = 0ull; rc == YVEX_OK && image < request->condition_count; ++image) {
        float *output = request->condition_latents + image * 24ull * spatial;
        h3_torch_normals(noise, 24ull * spatial, request->posterior_seed);
        for (channel = 0ull; channel < 24ull; ++channel)
            for (position = 0ull; position < spatial; ++position) {
                unsigned long long local = channel * spatial + position;
                unsigned long long base = image * 48ull * spatial;
                float mean = moments[base + local];
                float logvar = moments[base + (24ull + channel) * spatial + position];
                float sample;
                if (logvar < -30.0f) logvar = -30.0f;
                if (logvar > 20.0f) logvar = 20.0f;
                sample = mean + expf(0.5f * logvar) * noise[local];
                sample = yvex_quant_f16_decode(yvex_quant_f16_encode(sample));
                output[local] = (sample - request->latent_mean[channel]) /
                                request->latent_std[channel];
            }
        if (!h3_latent_identity(output, 24ull * spatial,
                                result->latent_identities[image]))
            rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.vae.identity",
                            "Visual VAE latent identity could not be sealed");
    }
    if (rc == YVEX_OK &&
        !yvex_sha256_hex_valid(request->video_component->residency_identity))
        rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.vae.evidence",
                        "Visual VAE residency evidence is incomplete");
    if (rc == YVEX_OK) {
        result->condition_count = request->condition_count;
        result->condition_rows = request->condition_count *
                                 (latent_height / 2ull) * (latent_width / 2ull);
        result->latent_channels = 24ull;
        result->latent_height = latent_height;
        result->latent_width = latent_width;
        result->latent_values = latent_values;
        result->resident_bytes = request->video_component->resident_encoded_bytes;
        result->kernel_launches = run.kernel_launches;
        result->h2d_bytes = run.h2d_bytes;
        result->d2h_bytes = run.d2h_bytes;
        result->device_bytes = run.peak_device_bytes;
        result->peak_workspace_bytes = run.peak_device_bytes;
        memcpy(result->residency_identity, request->video_component->residency_identity,
               sizeof(result->residency_identity));
        for (image = 0ull; image < request->condition_count; ++image)
            memcpy(result->media_identities[image], images.canvas[image].content_identity,
                   sizeof(result->media_identities[image]));
        if (!h3_keyframe_result_identity(result, result->execution_identity))
            rc = h3_refuse(err, YVEX_ERR_STATE, "minimax-h3.vae.identity",
                            "Visual VAE execution identity could not be sealed");
        else result->complete = 1;
    }
    free(noise); free(moments); free(pixels);
    h3_images_close(&images);
    if (rc != YVEX_OK) memset(result, 0, sizeof(*result));
    return rc;
}
