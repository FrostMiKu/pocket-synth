#pragma once
#include <cstdint>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace ps {
// Flush inaudible subnormal tails on this audio thread. This prevents filters
// from switching to the processor's slow path as their excitation fades.
inline void configureAudioFloat() {
#if defined(__aarch64__)
    uint64_t control;
    __asm__ volatile("mrs %0, fpcr" : "=r"(control));
    control|=uint64_t(1)<<24;
    __asm__ volatile("msr fpcr, %0" : : "r"(control));
#elif defined(__SSE__)
    _mm_setcsr(_mm_getcsr()|(1u<<15)|(1u<<6));
#endif
}
}
