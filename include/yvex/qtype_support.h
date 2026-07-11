/*
 * YVEX - Qtype support matrix
 */
#ifndef YVEX_QTYPE_SUPPORT_H
#define YVEX_QTYPE_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned int ggml_type;
    int policy_supported;
    int emit_supported;
    int quantize_supported;
    int compute_supported;
    const char *notes;
} yvex_qtype_support_info;

const yvex_qtype_support_info *yvex_qtype_support_by_name(const char *qtype);
unsigned long long yvex_qtype_support_count(void);
const yvex_qtype_support_info *yvex_qtype_support_at(unsigned long long index);
const char *yvex_qtype_support_name(const yvex_qtype_support_info *info);
int yvex_qtype_support_storage_supported(const yvex_qtype_support_info *info);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_QTYPE_SUPPORT_H */
