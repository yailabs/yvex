/*
 * YVEX - Qtype support matrix
 */
#include <string.h>

#include <yvex/qtype_support.h>

static const yvex_qtype_support_info qtype_rows[] = {
    {"F32",      1, 1, 1, 0, 1, "scalar emit supported; CPU/CUDA materialization only"},
    {"F16",      1, 1, 1, 1, 0, "scalar emit and cast supported; no compute claim"},
    {"BF16",     1, 1, 1, 1, 0, "scalar emit and cast supported; no compute claim"},
    {"Q8_0",     1, 1, 0, 0, 0, "storage known; quantizer/emitter not enabled in open-weight intake"},
    {"Q4_K",     1, 0, 0, 0, 0, "policy vocabulary only; no emitter"},
    {"Q2_K",     1, 0, 0, 0, 0, "policy vocabulary only; no emitter"},
    {"IQ2_XXS",  1, 0, 0, 0, 0, "policy vocabulary only; no emitter"},
};

const yvex_qtype_support_info *yvex_qtype_support_by_name(const char *qtype)
{
    unsigned long long i;
    if (!qtype) return NULL;
    for (i = 0; i < yvex_qtype_support_count(); ++i) {
        if (strcmp(qtype_rows[i].qtype, qtype) == 0) {
            return &qtype_rows[i];
        }
    }
    return NULL;
}

unsigned long long yvex_qtype_support_count(void)
{
    return (unsigned long long)(sizeof(qtype_rows) / sizeof(qtype_rows[0]));
}

const yvex_qtype_support_info *yvex_qtype_support_at(unsigned long long index)
{
    if (index >= yvex_qtype_support_count()) return NULL;
    return &qtype_rows[index];
}
