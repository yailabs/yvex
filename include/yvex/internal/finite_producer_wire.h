/* Versioned finite-decision payload within the existing local host frames. */
#ifndef INCLUDE_YVEX_INTERNAL_FINITE_PRODUCER_WIRE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_FINITE_PRODUCER_WIRE_H_INCLUDED

#include <yvex/finite_decision_producer.h>
#include <stddef.h>

int yvex_finite_producer_request_encode(const yvex_finite_producer_request *,
    unsigned char *, size_t, size_t *, yvex_error *);
int yvex_finite_producer_request_decode(const unsigned char *, size_t,
    yvex_finite_producer_request *, yvex_error *);
int yvex_finite_producer_result_encode(const yvex_finite_producer_result *,
    unsigned char *, size_t, size_t *, yvex_error *);
int yvex_finite_producer_result_decode(const unsigned char *, size_t,
    yvex_finite_producer_result *, yvex_error *);

#endif
