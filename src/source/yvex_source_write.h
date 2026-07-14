/*
 * yvex_source_write.h - source sidecar writer declarations.
 */
#ifndef YVEX_SOURCE_WRITE_H
#define YVEX_SOURCE_WRITE_H

#include <yvex/source_manifest.h>

#include "yvex_source_inventory.h"

struct yvex_source_payload_session;
#include "yvex_source_verify.h"

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
