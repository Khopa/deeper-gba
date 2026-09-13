// Unit test runner. Build & run with `make test`.
//   build/unit_tests            run everything
//   build/unit_tests -v         list every test as it runs
//   build/unit_tests -f dig     run only tests whose suite or name contains "dig"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "host_shim.h"

int test_checks, test_failures, test_verbose;
const char *test_filter;
static const char *current_test;
static int current_test_failed;

void test_fail(const char *file, int line, const char *msg)
{
    if (!current_test_failed) printf("  FAIL %s\n", current_test);
    current_test_failed = 1;
    test_failures++;
    printf("       %s:%d: %s\n", file, line, msg);
}

void test_run_suite(const char *suite, const TestCase *cases, int count)
{
    int ran = 0, failed = 0;
    for (int i = 0; i < count; i++) {
        if (test_filter && !strstr(suite, test_filter) && !strstr(cases[i].name, test_filter))
            continue;
        host_reset();
        current_test = cases[i].name;
        current_test_failed = 0;
        cases[i].fn();
        ran++;
        failed += current_test_failed;
        if (test_verbose && !current_test_failed) printf("  ok   %s\n", cases[i].name);
    }
    if (ran) printf("%-10s %3d tests, %d failed\n", suite, ran, failed);
}

// One declaration per suite (tests/unit/test_<suite>.c)
void run_dig(void);
void run_engine(void);
void run_run(void);
void run_save(void);
void run_vein(void);
void run_ledger(void);
void run_tunnel(void);
void run_block(void);

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) test_verbose = 1;
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) test_filter = argv[++i];
        else { fprintf(stderr, "usage: %s [-v] [-f filter]\n", argv[0]); return 2; }
    }
    run_dig();
    run_engine();
    run_run();
    run_save();
    run_vein();
    run_ledger();
    run_tunnel();
    run_block();
    printf("\n%d checks, %d failure(s)\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
