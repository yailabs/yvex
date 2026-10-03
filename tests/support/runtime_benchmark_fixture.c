/* Consumer fixture: reuse the independently registered native benchmark test record. */
#include "tests/unit/runtime_benchmark.c"
int main(int argc, char **argv)
{
    yvex_runtime_benchmark_baseline record;
    yvex_runtime_benchmark_failure reason;
    yvex_runtime_benchmark_publication publication;
    yvex_error error;
    int status;
    if (argc != 3) return 2;
    fixture_record(&record);
    if (!strcmp(argv[2], "regressed")) {
        for (unsigned int i = 0; i < YVEX_RUNTIME_BENCHMARK_STATISTIC_COUNT; ++i)
            record.metrics.host_timing.values[i] *= 2;
    } else if (strcmp(argv[2], "same")) return 2;
    status = yvex_runtime_benchmark_baseline_seal(&record, &reason, &error);
    if (!status) status = yvex_runtime_benchmark_baseline_write(argv[1], &record,
        &publication, &reason, &error);
    if (status) { fprintf(stderr, "%s: %s\n", error.where, error.message); return 1; }
    return 0;
}
