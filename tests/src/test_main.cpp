#include "vhid_test.hpp"

#include <cstdio>

int main() {
    int failed_cases = 0;
    int total = 0;
    for (auto& tc : vhidtest::registry()) {
        ++total;
        std::printf("[ RUN      ] %s\n", tc.name);
        int before = vhidtest::stats().failed_assertions;
        try {
            tc.fn();
        } catch (const vhidtest::RequireFailed&) {
            // already counted
        } catch (const std::exception& e) {
            std::fprintf(stderr, "  unhandled exception: %s\n", e.what());
            ++vhidtest::stats().failed_assertions;
        }
        int new_fail = vhidtest::stats().failed_assertions - before;
        if (new_fail == 0) {
            std::printf("[       OK ] %s\n", tc.name);
        } else {
            std::printf("[  FAILED  ] %s (%d failed assertions)\n", tc.name, new_fail);
            ++failed_cases;
        }
    }
    std::printf("\n%d/%d cases passed (%d total assertions failed)\n",
                total - failed_cases, total, vhidtest::stats().failed_assertions);
    return failed_cases == 0 ? 0 : 1;
}
