/* Compiler-owned joint-modality block computation. Runtime owns invocation,
 * conditioning preparation, transactions and publication, not layer topology. */
#ifndef INCLUDE_YVEX_INTERNAL_JOINT_PROGRAM_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_JOINT_PROGRAM_H_INCLUDED
#include <yvex/internal/program_physical.h>
#include <yvex/internal/joint_transformer.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const yvex_transformer_joint_recipe *architecture;
    const char *source_identity;
    unsigned long long rows, timesteps, blocks;
    double normalization_epsilon;
    /* Zero disables stage inspection; otherwise one-based block index. */
    unsigned long long inspected_block;
    int block_results;
    /* Nonzero video_rows selects the complete input/refiner/time/block/head
     * program. These are source/admission facts, never backend model guesses. */
    unsigned long long video_rows, audio_rows, text_rows;
    unsigned long long position_axes, time_embedding_width;
    double maximum_period;
    int final_hidden_result;
    const yvex_transformer_linear_physical_plan *video_output_target, *audio_output_target;
    /* Optional ordered diagnostic publications, not retained model outputs.
     * Tag = scope << 56 | block << 8 | stage; stage COUNT denotes block completion. */
    int observe_blocks, observe_stages;
    yvex_transformer_joint_scope observed_scope;
    unsigned long long observed_block;
    yvex_transformer_joint_stage observed_stage;
} yvex_joint_program_recipe;

typedef struct yvex_joint_program {
    yvex_program_physical *physical;
    /* Complete components also expose the same module's prepare and step
     * entrypoints. Prepared results are ordinary typed computational values;
     * their retention/invalidation belongs to the runtime, not this compiler. */
    yvex_program_physical *preparation, *step;
    size_t block_result_count, stage_result_count;
    yvex_transformer_joint_stage stages[28];
} yvex_joint_program;

int yvex_joint_program_compile(yvex_joint_program **, const yvex_joint_program_recipe *, yvex_error *);
void yvex_joint_program_close(yvex_joint_program **);

#ifdef __cplusplus
}
#endif
#endif
