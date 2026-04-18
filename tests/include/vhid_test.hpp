// Tiny self-contained test framework. No external dependencies.
//
//   TEST_CASE("name") { ... REQUIRE(expr); CHECK(expr); ... }
//
// REQUIRE aborts the case on failure; CHECK records and continues.
#pragma once

#include <cstdio>
#include <exception>
#include <functional>
#include <string>
#include <vector>

namespace vhidtest {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> v;
    return v;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct Stats {
    int failed_assertions = 0;
};
inline Stats& stats() { static Stats s; return s; }

struct RequireFailed : std::exception {};

} // namespace vhidtest

#define VHID_CONCAT_(a,b) a##b
#define VHID_CONCAT(a,b)  VHID_CONCAT_(a,b)

#define TEST_CASE(NAME)                                                        \
    static void VHID_CONCAT(test_fn_, __LINE__)();                             \
    static ::vhidtest::Registrar VHID_CONCAT(test_reg_, __LINE__)              \
        (NAME, &VHID_CONCAT(test_fn_, __LINE__));                              \
    static void VHID_CONCAT(test_fn_, __LINE__)()

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::fprintf(stderr, "  CHECK failed: %s  @ %s:%d\n",              \
                         #expr, __FILE__, __LINE__);                           \
            ++::vhidtest::stats().failed_assertions;                           \
        }                                                                      \
    } while (0)

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::fprintf(stderr, "  REQUIRE failed: %s  @ %s:%d\n",            \
                         #expr, __FILE__, __LINE__);                           \
            ++::vhidtest::stats().failed_assertions;                           \
            throw ::vhidtest::RequireFailed{};                                 \
        }                                                                      \
    } while (0)
