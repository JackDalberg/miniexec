#pragma once

#include <format>
#include <iostream>
#include <ostream>
#include <print>
#include <streambuf>

// ret (name)(param)
using TestFn = void (*)();

struct TestEntry {
  const char* group;
  const char* name;
  TestFn fn;
};

struct FailureMessage {
  const char* file;
  int line;
  std::string expected;
  std::string got;
};

constexpr int kMaxTestEntries = 256;
constexpr int kMaxFailuresPerTest = 32;

inline TestEntry g_tests[kMaxTestEntries];
inline int g_test_count = 0;

inline FailureMessage g_failures[kMaxFailuresPerTest];
inline int g_failure_count = 0;

inline bool register_test(const char* group, const char* name, TestFn fn) {
  g_tests[g_test_count] = TestEntry{group, name, fn};
  g_test_count++;
  return true;
}

// Taking inspiration from Google test.
// For more details, see
// https://github.com/google/googletest/blob/main/googletest/include/gtest/internal
#define TEST(group, name)                                   \
  static void test_##group##_##name(void);                  \
  static bool reg_##group##_##name =                        \
      register_test(#group, #name, &test_##group##_##name); \
  static void test_##group##_##name(void)

#define EXPECT_EQUAL(expected, got)                                            \
  do {                                                                         \
    const auto& lhs = (expected);                                              \
    const auto& rhs = (got);                                                   \
    if (lhs != rhs) {                                                          \
      g_failures[g_failure_count] = FailureMessage{                            \
          __FILE__, __LINE__, std::format("{}", lhs), std::format("{}", rhs)}; \
      g_failure_count++;                                                       \
    }                                                                          \
  } while (0)

inline int RUN_ALL_TESTS() {
  int failed = 0;
  for (int i = 0; i < g_test_count; i++) {
    g_failure_count = 0;
    const auto& test = g_tests[i];
    test.fn();

    if (g_failure_count != 0) {
      failed++;
      std::printf("[FAIL] Test.%s.%s\n", test.group, test.name);
      for (int j = 0; j < g_failure_count; j++) {
        const auto& fail = g_failures[j];
        std::printf("\t%s:%d\n\t\texpected: %s\n\t\tgot: %s\n", fail.file,
                    fail.line, fail.expected.c_str(), fail.got.c_str());
      }
    } else {
      std::printf("[ OK ] Test.%s.%s\n", test.group, test.name);
    }
  }
  return failed > 0 ? 1 : 0;
}

// Helpers since most of the std::execution examples use std::cout.
struct CoutCapturer {
  std::streambuf* old;

  CoutCapturer(std::streambuf* new_buf) : old(std::cout.rdbuf(new_buf)) {}
  ~CoutCapturer() { std::cout.rdbuf(old); }
};
