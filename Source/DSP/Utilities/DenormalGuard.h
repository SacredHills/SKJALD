#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
  #include <xmmintrin.h>
  #define SKJALD_HAS_SSE_CSR 1
#endif

namespace skjald
{
/** RAII guard enabling flush-to-zero / denormals-are-zero for the current thread.
    Used by the DSP engine when it runs outside JUCE (tests, offline tool). Inside the
    plugin, juce::ScopedNoDenormals is used as well; nesting is harmless. */
class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals() noexcept
    {
#if defined(SKJALD_HAS_SSE_CSR)
        previous = _mm_getcsr();
        _mm_setcsr ((unsigned int) previous | 0x8040u); // FTZ | DAZ
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        uint64_t fpcr;
        asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
        previous = fpcr;
        asm volatile ("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
    }

    ~ScopedFlushDenormals() noexcept
    {
#if defined(SKJALD_HAS_SSE_CSR)
        _mm_setcsr ((unsigned int) previous);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        asm volatile ("msr fpcr, %0" : : "r"(previous));
#endif
    }

    ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

private:
    uint64_t previous = 0;
};

} // namespace skjald
