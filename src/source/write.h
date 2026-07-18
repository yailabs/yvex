/*
 * Owner: source.write (source).
 * Owns: the private-interface boundary consumed by model,compilation.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: private-interface; moving this contract requires an ownership-manifest change.
 *
 * write.h - source sidecar writer declarations.
 */
#ifndef YVEX_SOURCE_WRITE_H
#define YVEX_SOURCE_WRITE_H

#include <yvex/source_manifest.h>

#include "inventory.h"

struct yvex_source_payload_session;
#include "verify.h"

int yvex_source_manifest_publish_verified(
    const char *out_path,
    const yvex_source_verify_options *options,
    const yvex_source_verification *verification,
    yvex_error *err);
int yvex_source_derived_inventory_publish(
    const char *out_path,
    const yvex_source_verify_options *options,
    const yvex_source_derived_inventory *inventory,
    yvex_error *err);
int yvex_source_manifest_publish_payload(
    const char *out_path,
    const yvex_source_verification *verification,
    const struct yvex_source_payload_session *session,
    yvex_error *err);

#endif
