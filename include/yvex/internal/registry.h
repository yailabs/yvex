/* Caller-owned model registry operations for native application consumers.
 * These records are private ABI, not CLI argv or a persisted format. */
#ifndef INCLUDE_YVEX_INTERNAL_REGISTRY_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_REGISTRY_H_INCLUDED

#include <yvex/artifact.h>
#include <yvex/registry.h>

#ifdef __cplusplus
extern "C" {
#endif

/* entry strings borrow these buffers until the caller moves or retires storage. */
typedef struct {
    yvex_model_registry_entry entry;
    char alias[256], family[128], model[128], scope[64], artifact_class[128];
    char qprofile[64], calibration[128], producer[64], artifact_schema[64];
    char path[YVEX_ARTIFACT_PATH_CAP];
} yvex_model_registry_derivation;
int yvex_model_registry_derive(yvex_model_registry_derivation *out,
                               const char *path, yvex_error *err);

typedef struct {
    char alias[256], sha256[YVEX_SHA256_HEX_CAP];
    unsigned long long file_size;
} yvex_model_registry_creation;
/* Missing naming fields may be derived by the canonical filename grammar.
 * Identity and metadata come from the file, not a caller claim. No registry
 * change is saved on identity, metadata or startup-profile refusal. */
int yvex_model_registry_create(const yvex_model_registry_entry *requested,
                                const char *expected_sha256,
                                const char *registry_path, int replace_existing,
                                yvex_model_registry_creation *out, yvex_error *err);

/* Remove under the same native transaction lock as creation. When provided,
 * the expected immutable package digest must match the current alias. */
int yvex_model_registry_remove_exact(const char *alias, const char *expected_sha256,
                                      const char *registry_path, yvex_error *err);

typedef struct {
    int passed, metadata_checked;
    char identity_status[24], metadata_status[24], readiness_status[24];
    char status[48], reason[YVEX_ERROR_MESSAGE_CAP];
    yvex_artifact_file_identity identity;
    yvex_model_metadata_snapshot current;
    yvex_model_metadata_drift_report drift;
} yvex_model_registry_verification;
/* current.entry borrows the caller-owned current buffers. Failure results may
 * be inspected, but never promote registration into runtime readiness. */
int yvex_model_registry_verify(const yvex_model_registry_entry *entry,
                                yvex_model_registry_verification *out, yvex_error *err);

/* Reconcile metadata with this exact integrity result, including a recipient's
 * expected digest. Do not reopen/re-hash independently and replace its admission.
 * out.current borrows out's storage. A failed result remains inspectable. */
int yvex_model_ref_verify_integrity(const yvex_model_ref *ref,
                                    const yvex_artifact_integrity_report *integrity,
                                    yvex_model_registry_verification *out, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
