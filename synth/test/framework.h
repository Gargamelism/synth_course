#pragma once

// Minimal host-side test helper — no external dependency, just enough to
// assert and report a pass/fail summary. See test/README.md.

#include <cmath>
#include <cstdio>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                 \
  do {                                                              \
    g_checks++;                                                     \
    if (!(cond)) {                                                  \
      g_failures++;                                                 \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                \
  } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

#define TEST_MAIN()                                                          \
  int main() {                                                               \
    runTests();                                                              \
    printf("%s: %d/%d checks passed\n", __FILE__, g_checks - g_failures, g_checks); \
    return g_failures == 0 ? 0 : 1;                                          \
  }
