/*
 * YVEX - Weight mapping report helpers
 *
 * File: yvex_weight_mapping_report.c
 * Layer: tool-plane implementation
 */
#include "yvex_weight_mapping_internal.h"

#include <stdio.h>

void yvex_weight_mapping_print_shape(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    if (!dims || rank == 0) {
        printf("unknown");
        return;
    }
    printf("[");
    for (i = 0; i < rank; ++i) {
        if (i) printf(",");
        printf("%llu", dims[i]);
    }
    printf("]");
}
