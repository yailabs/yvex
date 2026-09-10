/* Exact CSA candidate ranking, including long prefixes and malformed history. */
#include <math.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <yvex/internal/backend.h>
#include "src/backend/cuda/private.h"
#include "tests/test.h"

enum { SELECTION_CAPACITY = 4096, SELECTION_HEADS = 512, SELECTION_WIDTH = 128 };
typedef struct {
    float query[SELECTION_HEADS * SELECTION_WIDTH], weights[SELECTION_HEADS];
    float rows[SELECTION_CAPACITY][SELECTION_WIDTH];
    unsigned long long positions[SELECTION_CAPACITY];
    unsigned long long selected[SELECTION_CAPACITY], selected_positions[SELECTION_CAPACITY];
    unsigned long long selected_count, valid_count;
    float scores[SELECTION_CAPACITY];
    unsigned long long indexes[SELECTION_CAPACITY];
    int status;
} selection_storage;

typedef struct {
    float score;
    unsigned long long index, position;
} selection_expected;

/* Independent host full sort, not the device sorting network. */
static int selection_compare(const void *left, const void *right)
{
    const selection_expected *a = left, *b = right;
    if (a->score != b->score) return a->score > b->score ? -1 : 1;
    return a->position < b->position ? -1 : a->position != b->position;
}

static unsigned long long selection_now(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0ull;
    return (unsigned long long)now.tv_sec * 1000000000ull +
           (unsigned long long)now.tv_nsec;
}

/* Scalar source-order F64 oracle, independent of device launch/sort ownership. */
static float selection_score(const selection_storage *s, unsigned long long candidate,
                             unsigned long long heads, unsigned long long width)
{
    double score = 0.0;
    for (unsigned long long head = 0ull; head < heads; ++head) {
        double dot = 0.0;
        for (unsigned long long lane = 0ull; lane < width; ++lane)
            dot += (double)s->query[head * width + lane] * (double)s->rows[candidate][lane];
        if (dot < 0.0) dot = 0.0;
        score += dot * (double)s->weights[head];
    }
    score *= 1.0 / sqrt((double)width);
    score *= 1.0 / sqrt((double)heads);
    return (float)score;
}

static int selection_case(yvex_backend *backend, unsigned long long count,
                          unsigned long long k, unsigned int scenario)
{
    selection_storage *host = calloc(1u, sizeof(*host));
    selection_storage *observed = calloc(1u, sizeof(*observed));
    selection_expected expected[SELECTION_CAPACITY];
    yvex_backend_tensor_desc descriptor = {0};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long history_count = count / 2ull, current_count = count - history_count;
    unsigned long long heads = scenario >= 8u ? 64ull : 4ull;
    unsigned long long width = scenario >= 8u ? 128ull : 4ull, stride = SELECTION_WIDTH, ratio = 4ull;
    unsigned long long query_position = count ? count * ratio - 1ull : 0ull;
    unsigned long long valid = 0ull, start, elapsed, score_elapsed, extent = 1ull;
    CUdeviceptr base, query, weights, history, history_positions, current, current_positions;
    CUdeviceptr selected, selected_positions, selected_count, valid_count, scores, indexes, status;
    yvex_error err;
    int rc, device_wide = 0;
    YVEX_TEST_ASSERT(host && observed, "allocate candidate ranking oracle");
    while (extent < count) extent *= 2ull;
    if (scenario == 9u) heads = 512ull; /* More than one reduction tile. */
    for (unsigned long long head = 0ull; head < heads; ++head) {
        host->query[head * width] = (float)(head + 1ull);
        host->weights[head] = scenario == 1u ? -1.0f : 1.0f;
        if (scenario >= 8u) for (unsigned long long lane = 0ull; lane < width; ++lane)
            host->query[head * width + lane] = (float)((int)((head * 11ull + lane * 7ull) % 31ull) - 15) / 16.0f;
    }
    for (unsigned long long index = 0ull; index < count; ++index) {
        float value = (float)((int)((index * 17ull) % 63ull) - 31);
        host->rows[index][0] = value;
        host->positions[index] = (count - 1ull - index) * ratio;
        if (scenario >= 8u) for (unsigned long long lane = 0ull; lane < width; ++lane)
            host->rows[index][lane] = (float)((int)((index * 13ull + lane * 3ull) % 23ull) - 11) / 8.0f;
    }
    if (scenario == 2u) host->positions[count - 1ull] = query_position + 1ull;
    if (scenario == 3u) host->positions[count - 1ull] = host->positions[0];
    if (scenario == 4u) host->rows[count - 1ull][0] = NAN;
    if (scenario == 5u) host->status = 1;
    if (scenario == 6u) host->positions[count - 1ull] = ~0ull;
    if (scenario == 7u) k = 0ull;
    if (scenario == 10u) for (unsigned long long index = 1ull; index < count; index += 2ull)
        host->positions[index] = query_position + index;
    if (scenario == 11u) query_position = 0ull;
    if (scenario == 12u) host->positions[history_count] = host->positions[0];
    if (scenario == 13u) host->query[0] = NAN;
    if (scenario == 14u) host->weights[0] = INFINITY;
    if (scenario == 15u) {
        host->weights[0] = FLT_MAX;
        host->query[0] = FLT_MAX;
    }
    for (unsigned long long index = 0ull; index < count; ++index) {
        unsigned long long position = host->positions[index];
        if (position > query_position || position > ~0ull - ratio + 1ull ||
            position + ratio - 1ull > query_position) continue;
        float score = selection_score(host, index, heads, width);
        expected[valid++] = (selection_expected){score, index, position};
    }
    qsort(expected, (size_t)valid, sizeof(expected[0]), selection_compare);
    descriptor.name = "attention-selection";
    descriptor.dtype = YVEX_DTYPE_I8;
    descriptor.rank = 1u;
    descriptor.dims[0] = descriptor.bytes = sizeof(*host);
    YVEX_TEST_ASSERT(
        yvex_backend_tensor_alloc(backend, &descriptor, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, host, sizeof(*host), &err) == YVEX_OK,
        "upload candidate ranking fixture");
    base = yvex_cuda_activation_pointer(backend, arena);
    query = base + offsetof(selection_storage, query);
    weights = base + offsetof(selection_storage, weights);
    history = base + offsetof(selection_storage, rows);
    history_positions = base + offsetof(selection_storage, positions);
    current = history + history_count * stride * sizeof(float);
    current_positions = history_positions + history_count * sizeof(unsigned long long);
    selected = base + offsetof(selection_storage, selected);
    selected_positions = base + offsetof(selection_storage, selected_positions);
    selected_count = base + offsetof(selection_storage, selected_count);
    valid_count = base + offsetof(selection_storage, valid_count);
    scores = base + offsetof(selection_storage, scores);
    indexes = base + offsetof(selection_storage, indexes);
    status = base + offsetof(selection_storage, status);
    start = selection_now();
    {
        void *params[] = {
            &query, &weights, &history, &history_positions, &history_count, &stride,
            &current, &current_positions, &current_count, &stride, &heads, &width,
            &ratio, &query_position, &scores, &status
        };
        rc = yvex_cuda_launch(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->attention_candidate_scores_function, count ? (unsigned int)count : 1u,
            256u, 256u * sizeof(double), params, "cuda.test.attention-score", &err);
    }
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend,
        YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &device_wide, "cuda.test.attention-score", &err);
    score_elapsed = selection_now() - start;
    if (rc == YVEX_OK) {
        void *params[] = {&history_positions, &history_count, &current_positions, &current_count,
            &ratio, &query_position, &k, &selected, &selected_positions, &selected_count,
            &valid_count, &scores, &indexes, &extent, &status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->attention_topk_function, 1u, 256u, 0u, params, "cuda.test.attention-selection", &err);
    }
    if (rc == YVEX_OK)
        rc = yvex_cuda_launch_synchronize(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &device_wide,
            "cuda.test.attention-selection", &err);
    elapsed = selection_now() - start;
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
        yvex_backend_tensor_read(backend, arena, observed, sizeof(*observed), &err) == YVEX_OK,
        "read candidate selection");
    if (scenario == 3u || scenario == 4u || scenario == 5u || scenario == 7u || scenario >= 12u) {
        YVEX_TEST_ASSERT(observed->status != 0 && observed->selected_count == 0ull &&
                         observed->valid_count == 0ull,
                         "duplicate/nonfinite/prior error/invalid K cannot publish candidate ranking");
    } else {
        unsigned long long chosen = valid < k ? valid : k;
        YVEX_TEST_ASSERT(observed->status == 0 && observed->valid_count == valid &&
                         observed->selected_count == chosen, "exact visible candidate counts");
        for (unsigned long long index = 0ull; index < chosen; ++index)
            YVEX_TEST_ASSERT(observed->selected[index] == expected[index].index &&
                             observed->selected_positions[index] == expected[index].position,
                             "full-sort oracle: exact score order and position tie break");
        unsigned char seen[SELECTION_CAPACITY] = {0};
        for (unsigned long long index = 0ull; index < valid; ++index) {
            unsigned long long candidate = observed->indexes[index];
            YVEX_TEST_ASSERT(candidate < count && !seen[candidate], "scratch preserves distinct candidate identities");
            seen[candidate] = 1u;
            YVEX_TEST_ASSERT(host->positions[candidate] <= query_position &&
                host->positions[candidate] <= ~0ull - ratio + 1ull &&
                host->positions[candidate] + ratio - 1ull <= query_position,
                "scratch contains only admitted visible candidates");
            float score = selection_score(host, candidate, heads, width);
            YVEX_TEST_ASSERT(observed->scores[index] == score, "ranking leaves each candidate's exact score intact");
        }
    }
    YVEX_TEST_ASSERT(!memcmp(host->query, observed->query, sizeof(host->query)) &&
        !memcmp(host->weights, observed->weights, sizeof(host->weights)) &&
        !memcmp(host->rows, observed->rows, sizeof(host->rows)) &&
        !memcmp(host->positions, observed->positions, sizeof(host->positions)),
        "ranking never mutates immutable inputs or state");
    for (unsigned long long index = extent; index < SELECTION_CAPACITY; ++index)
        YVEX_TEST_ASSERT(observed->scores[index] == 0.0f && observed->indexes[index] == 0ull,
            "ranking stays inside the admitted padded scratch extent");
    printf("attention selection: candidates=%llu k=%llu scenario=%u selected=%llu "
           "valid=%llu status=%d %s heads=%llu width=%llu score_sync_ns=%llu rank_sync_ns=%llu total_ns=%llu\n",
           count, k, scenario, observed->selected_count, observed->valid_count,
           observed->status, observed->status ? "publication=refused" :
           "mismatches=0 score_max_abs=0 tolerance=0", heads, width, score_elapsed, elapsed - score_elapsed, elapsed);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
                     "release candidate ranking fixture");
    free(observed);
    free(host);
    return 0;
}

int yvex_cuda_test_attention_selection(void)
{
    const unsigned long long counts[] = {0ull, 1ull, 3ull, 5ull, 64ull, 129ull, 255ull, 256ull,
        257ull, 511ull, 512ull, 513ull, 1023ull, 1024ull, 1025ull, 2047ull, 2048ull, 2049ull, 4095ull, 4096ull};
    yvex_backend_options options = {0};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc;
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open selection CUDA backend");
    for (size_t i = 0u; i < sizeof(counts) / sizeof(counts[0]); ++i)
        if (selection_case(backend, counts[i], 512ull, 0u)) return 1;
    for (unsigned int scenario = 1u; scenario <= 7u; ++scenario)
        if (selection_case(backend, 64ull, 17ull, scenario)) return 1;
    for (unsigned int repeat = 0u; repeat < 3u; ++repeat)
        for (unsigned long long count = 64ull; count <= 4096ull; count *= 2ull)
            if (selection_case(backend, count, 512ull, 8u)) return 1;
    if (selection_case(backend, 64ull, 17ull, 9u)) return 1;
    for (unsigned int scenario = 10u; scenario <= 15u; ++scenario)
        if (selection_case(backend, 513ull, 17ull, scenario)) return 1;
    yvex_backend_close(backend);
    return 0;
}
