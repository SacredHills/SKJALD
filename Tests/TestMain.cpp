#include "TestFramework.h"

#include <chrono>
#include <cstring>

int main (int argc, char** argv)
{
    using namespace skjald::test;
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;

    for (auto& t : registry())
    {
        const std::string full = t.suite + "/" + t.name;
        if (filter != nullptr && full.find (filter) == std::string::npos)
            continue;
        state().current = full;
        const int before = state().failures;
        std::printf ("[ RUN  ] %s\n", full.c_str());
        std::fflush (stdout);
        const auto t0 = std::chrono::steady_clock::now();
        t.fn();
        const auto ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        const bool ok = state().failures == before;
        std::printf ("[ %s ] %s (%.0f ms)\n", ok ? " OK " : "FAIL", full.c_str(), ms);
        std::fflush (stdout);
        ++run;
        if (! ok)
            ++failedTests;
    }

    std::printf ("\n%d tests, %d checks, %d failed tests, %d failed checks\n", run, state().checks, failedTests, state().failures);
    return failedTests == 0 ? 0 : 1;
}
