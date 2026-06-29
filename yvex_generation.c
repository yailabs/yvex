/*
 * yvex_generation.c - Runtime generation loop boundary.
 *
 * This file will own the constrained generation loop only after decode, logits,
 * and sampling exist as tested runtime behavior. CLI and server generation
 * surfaces must call this runtime path instead of duplicating generation logic.
 */

#include <yvex/yvex.h>
