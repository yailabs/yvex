/*
 * yvex_decode.c - Decode-step runtime boundary.
 *
 * This file will own decode-step behavior over existing KV-backed transformer
 * state. It must not invent state. Decode becomes meaningful only after real
 * transformer prefill has produced runtime state worth advancing.
 */

#include <yvex/yvex.h>
