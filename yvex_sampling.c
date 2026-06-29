/*
 * yvex_sampling.c - Sampling runtime boundary.
 *
 * This file will own deterministic and stochastic sampling over real logits.
 * It must not claim sampling, generation, or provider output before logits
 * exist and are tested through the runtime path.
 */

#include <yvex/yvex.h>
