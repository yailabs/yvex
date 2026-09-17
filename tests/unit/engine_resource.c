/* Prove independent engine-resource lifetime without introducing a model-specific cache. */
#include <yvex/internal/engine_resource.h>

#include <string.h>

#include "tests/test.h"

static const char package_identity[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const char engine_identity[] =
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
static const char specialization_identity[] =
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
static const char admission_identity[] =
    "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

typedef struct {
    unsigned int releases;
    int fail;
    yvex_engine_resource_catalog *grow_during_release;
} release_probe;

static int release_count(void *context, yvex_error *err)
{
    release_probe *probe = context;
    probe->releases++;
    if (probe->grow_during_release) {
        unsigned long long bytes;
        int rc = yvex_runtime_resource_catalog_reserve(
            probe->grow_during_release, 8u, 0u, &bytes, err);
        if (rc != YVEX_OK) return rc;
    }
    if (probe->fail) {
        yvex_error_set(err, YVEX_ERR_STATE, "test.engine-resource",
                       "injected resource release failure");
        return YVEX_ERR_STATE;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_test_engine_resource(void)
{
    yvex_engine_resource_catalog *catalog = NULL;
    yvex_engine_resource_handle package = {0}, prepared = {0}, stale;
    yvex_engine_resource_request request = {0};
    yvex_engine_resource_summary summary = {0};
    yvex_engine_resource_entry entries[2] = {0};
    release_probe package_probe = {0}, prepared_probe = {0};
    unsigned long long count = 0ull;
    unsigned long long metadata = 0u;
    void *borrowed = NULL;
    yvex_error err;

    YVEX_TEST_ASSERT(
        yvex_runtime_resource_catalog_open(
            &catalog, 17ull, engine_identity, 4ull, &err) == YVEX_OK,
        "engine resource catalog opens for one authenticated generation");
    request.kind = YVEX_ENGINE_RESOURCE_PACKAGE_MAPPING;
    request.owner = YVEX_ENGINE_RESOURCE_OWNER_PACKAGE;
    request.lifetime = YVEX_ENGINE_RESOURCE_LIFETIME_ENGINE;
    request.numeric_class = YVEX_ENGINE_RESOURCE_NUMERIC_CANONICAL_PACKAGE;
    request.name = "canonical-package";
    request.package_identity = package_identity;
    request.bytes.mapped_package_bytes = 4096ull;
    request.value = &package_probe;
    request.release = release_count;
    request.release_context = &package_probe;
    request.ready = 1;
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_register(catalog, &request, &package, &err) ==
            YVEX_OK,
        "canonical package mapping registers independently");

    memset(&request, 0, sizeof(request));
    request.kind = YVEX_ENGINE_RESOURCE_PREPARED_LAYOUT;
    request.owner = YVEX_ENGINE_RESOURCE_OWNER_SPECIALIZATION;
    request.lifetime = YVEX_ENGINE_RESOURCE_LIFETIME_REQUEST;
    request.numeric_class = YVEX_ENGINE_RESOURCE_NUMERIC_EQUIVALENT_PREPARED;
    request.name = "synthetic-layout";
    request.package_identity = package_identity;
    request.specialization_identity = specialization_identity;
    request.admission_identity = admission_identity;
    request.dependency = package;
    request.bytes.host_resident_bytes = 256ull;
    request.bytes.prepared_bytes = 256ull;
    request.preparation_nanoseconds = 700ull;
    request.value = &prepared_probe;
    request.release = release_count;
    request.release_context = &prepared_probe;
    request.evictable = 1;
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_register(catalog, &request, &prepared, &err) ==
            YVEX_OK,
        "one independently evictable prepared view registers");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_snapshot(
            catalog, &summary, entries, 2ull, &count, &err) == YVEX_OK &&
            count == 2ull && summary.resource_count == 2ull &&
            summary.ready_count == 1ull &&
            summary.bytes.mapped_package_bytes == 4096ull &&
            summary.bytes.prepared_bytes == 256ull &&
            entries[0].dependent_count == 1ull &&
            entries[1].state == YVEX_ENGINE_RESOURCE_DECLARED,
        "snapshot separates declared prepared work from ready package truth");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_acquire(catalog, prepared, &borrowed, &err) ==
            YVEX_ERR_STATE,
        "declared resource cannot be consumed before readiness publication");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_publish_ready(catalog, prepared, &err) ==
            YVEX_OK,
        "prepared resource publishes readiness without changing package identity");

    YVEX_TEST_ASSERT(
        yvex_runtime_resource_acquire(catalog, prepared, &borrowed, &err) ==
                YVEX_OK &&
            borrowed == &prepared_probe,
        "consumer borrows an exact resource generation");
    YVEX_TEST_ASSERT(yvex_runtime_resource_catalog_reserve(catalog, 6u, 1u, &metadata, &err) ==
        YVEX_ERR_BOUNDS && !metadata &&
        yvex_runtime_resource_snapshot(catalog, &summary, entries, 2u, &count, &err) == YVEX_OK &&
        summary.capacity == 4u && entries[1].consumer_count == 1u,
        "failed growth preserves live metadata and the borrowed resource");
    YVEX_TEST_ASSERT(yvex_runtime_resource_catalog_reserve(catalog, 6u, 0u, &metadata, &err) == YVEX_OK &&
        metadata && yvex_runtime_resource_snapshot(catalog, &summary, entries, 2u, &count, &err) == YVEX_OK &&
        summary.capacity == 6u && count == 2u && entries[1].consumer_count == 1u &&
        entries[0].dependent_count == 1u && borrowed == &prepared_probe &&
        entries[1].handle.generation == prepared.generation && entries[1].handle.slot == prepared.slot,
        "metadata growth preserves resource identity, dependency and borrowed value");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_evict(catalog, &prepared, &err) ==
                YVEX_ERR_STATE &&
            prepared_probe.releases == 0u,
        "borrowed prepared resource cannot be evicted");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_drop(catalog, prepared, &err) == YVEX_OK,
        "consumer discharges the exact borrow");
    stale = prepared;
    prepared_probe.grow_during_release = catalog;
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_evict(catalog, &prepared, &err) == YVEX_OK &&
            !prepared.engine_generation && prepared_probe.releases == 1u,
        "prepared resource evicts independently from package truth");
    YVEX_TEST_ASSERT(yvex_runtime_resource_snapshot(catalog, &summary, entries, 2u, &count, &err) == YVEX_OK &&
        summary.capacity == 8u && count == 1u,
        "release callback may grow metadata; release resolves its handle again after callback");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_acquire(catalog, stale, &borrowed, &err) ==
            YVEX_ERR_STATE,
        "evicted generation handle is stale");
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_snapshot(
            catalog, &summary, entries, 2ull, &count, &err) == YVEX_OK &&
            count == 1ull && summary.resource_count == 1ull &&
            summary.eviction_count == 1ull &&
            summary.acquisition_count == 1ull &&
            summary.preparation_nanoseconds == 700ull &&
            summary.bytes.mapped_package_bytes == 4096ull &&
            summary.bytes.prepared_bytes == 0ull,
        "eviction releases prepared accounting without changing package mapping");

    package_probe.fail = 1;
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_catalog_close(&catalog, &err) ==
                YVEX_ERR_STATE &&
            catalog && package_probe.releases == 1u &&
            yvex_runtime_resource_snapshot(
                catalog, &summary, entries, 2ull, &count, &err) == YVEX_OK &&
            summary.failed_count == 1ull && !summary.ready_count &&
            summary.failed_release_count == 1ull,
        "failed release preserves exact ownership for a retry");
    YVEX_TEST_ASSERT(yvex_runtime_resource_catalog_reserve(catalog, 16u, 0u, &metadata, &err) ==
        YVEX_ERR_STATE && !metadata, "closing resource catalogs refuse growth without losing cleanup");
    package_probe.fail = 0;
    YVEX_TEST_ASSERT(
        yvex_runtime_resource_catalog_close(&catalog, &err) == YVEX_OK &&
            !catalog && package_probe.releases == 2u,
        "catalog retries and closes the remaining resource exactly");
    return 0;
}
