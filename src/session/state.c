/*
 * YVEX - Session state names
 *
 * File: src/session/state.c
 * Layer: session implementation
 *
 * Purpose:
 *   Provides stable string names for the H0 session state machine.
 *
 * Implements:
 *   - yvex_session_state_name
 *
 * Invariants:
 *   - state names are lowercase CLI-safe labels
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_session
 */
#include "session_internal.h"

const char *yvex_session_state_name(yvex_session_state state)
{
    switch (state) {
    case YVEX_SESSION_STATE_CREATED: return "created";
    case YVEX_SESSION_STATE_READY: return "ready";
    case YVEX_SESSION_STATE_PREFILLING: return "prefilling";
    case YVEX_SESSION_STATE_DECODING: return "decoding";
    case YVEX_SESSION_STATE_PARTIAL: return "partial";
    case YVEX_SESSION_STATE_CANCELLED: return "cancelled";
    case YVEX_SESSION_STATE_FAILED: return "failed";
    case YVEX_SESSION_STATE_CLOSED: return "closed";
    }
    return "unknown";
}
