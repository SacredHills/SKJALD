#pragma once

// Minimal self-contained test framework (no third-party dependency).

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace skjald::test
{
struct TestCase
{
    std::string suite, name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

struct Registrar
{
    Registrar (const char* suite, const char* name, std::function<void()> fn)
    {
        registry().push_back ({ suite, name, std::move (fn) });
    }
};

struct State
{
    int failures = 0;
    int checks = 0;
    std::string current;
};

inline State& state()
{
    static State s;
    return s;
}

inline void reportFailure (const char* file, int line, const std::string& msg)
{
    ++state().failures;
    std::printf ("    FAIL [%s] %s:%d: %s\n", state().current.c_str(), file, line, msg.c_str());
}

} // namespace skjald::test

#include <cstdarg>

namespace skjald::test
{
inline void info (const char* fmt, ...)
{
    std::va_list args;
    va_start (args, fmt);
    std::printf ("      ");
    std::vprintf (fmt, args);
    std::printf ("\n");
    va_end (args);
}
} // namespace skjald::test

#define SKJALD_CONCAT2(a, b) a##b
#define SKJALD_CONCAT(a, b) SKJALD_CONCAT2 (a, b)

#define TEST_CASE(suite, name)                                                                      \
    static void SKJALD_CONCAT (test_, __LINE__)();                                                  \
    static skjald::test::Registrar SKJALD_CONCAT (reg_, __LINE__) (suite, name, SKJALD_CONCAT (test_, __LINE__)); \
    static void SKJALD_CONCAT (test_, __LINE__)()

#define CHECK(cond)                                                                                 \
    do {                                                                                            \
        ++skjald::test::state().checks;                                                             \
        if (! (cond)) skjald::test::reportFailure (__FILE__, __LINE__, "CHECK(" #cond ")");         \
    } while (0)

#define CHECK_MSG(cond, ...)                                                                        \
    do {                                                                                            \
        ++skjald::test::state().checks;                                                             \
        if (! (cond)) {                                                                             \
            char buf_[512];                                                                         \
            std::snprintf (buf_, sizeof (buf_), __VA_ARGS__);                                       \
            skjald::test::reportFailure (__FILE__, __LINE__, std::string ("CHECK(" #cond ") ") + buf_); \
        }                                                                                           \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                                       \
    do {                                                                                            \
        ++skjald::test::state().checks;                                                             \
        const double a_ = (double) (a), b_ = (double) (b), t_ = (double) (tol);                     \
        if (! (std::abs (a_ - b_) <= t_)) {                                                         \
            char buf_[256];                                                                         \
            std::snprintf (buf_, sizeof (buf_), "%s = %.6f, expected %.6f +/- %.6f", #a, a_, b_, t_); \
            skjald::test::reportFailure (__FILE__, __LINE__, buf_);                                 \
        }                                                                                           \
    } while (0)

#define CHECK_LE(a, b)                                                                              \
    do {                                                                                            \
        ++skjald::test::state().checks;                                                             \
        const double a_ = (double) (a), b_ = (double) (b);                                          \
        if (! (a_ <= b_)) {                                                                         \
            char buf_[256];                                                                         \
            std::snprintf (buf_, sizeof (buf_), "%s = %.6f, expected <= %.6f", #a, a_, b_);         \
            skjald::test::reportFailure (__FILE__, __LINE__, buf_);                                 \
        }                                                                                           \
    } while (0)

#define CHECK_GE(a, b)                                                                              \
    do {                                                                                            \
        ++skjald::test::state().checks;                                                             \
        const double a_ = (double) (a), b_ = (double) (b);                                          \
        if (! (a_ >= b_)) {                                                                         \
            char buf_[256];                                                                         \
            std::snprintf (buf_, sizeof (buf_), "%s = %.6f, expected >= %.6f", #a, a_, b_);         \
            skjald::test::reportFailure (__FILE__, __LINE__, buf_);                                 \
        }                                                                                           \
    } while (0)
