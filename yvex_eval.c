/*
 * yvex_eval.c - Evaluation harness boundary.
 *
 * This file is not a unit-test runner and it is not a benchmark. It will own
 * runtime-facing evaluation harness code once the same generation path used by
 * operators exists. Until then, it remains a source ownership boundary only.
 *
 * Evaluation in YVEX must follow implemented runtime boundaries: fixture
 * checks are not model quality, logits regression is not capability eval, and
 * capability evaluation cannot start before runtime generation exists.
 */

#include <yvex/yvex.h>
