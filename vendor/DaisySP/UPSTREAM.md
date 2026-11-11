DaisySP https://github.com/electro-smith/DaisySP
Commit 2c72eaf9eac5fc0dca1919d65d606da907832618 (2026-09-28)
Subset: StringVoice, String, Oscillator, Adsr and dependencies. MIT.

Local adaptation: Utility/dsp.h adds RealtimeRandomUnit, a
thread-local xorshift32 source with constant initialization. Noise/dust.h,
PhysicalModeling/stringvoice.cpp and PhysicalModeling/KarplusString.cpp use
it instead of the process-wide libc rand source. This keeps the audio path
independent of libc's shared random-state lock. DSP models are preserved.

Local coefficient cache: PhysicalModeling/KarplusString caches
parameter-derived damping, pitch compensation and OnePole coefficients until
frequency, brightness or damping changes. Reset invalidates the cache. The
original equations, sample processing and control response are preserved. This
removes repeated powf/atanf/tanf calls from steady-state multi-track playback.

Local sine evaluation: Synthesis/sine_lookup.h supplies one
4096-point linearly interpolated sine cycle. Oscillator WAVE_SIN and vibrato
share it; all waveform phases and frequency controls remain continuous. The
immutable table is primed during electronic-kit construction before audio.

Local excitation pruning: StringVoice skips its excitation filter
after the finite noise burst has ended and both low/band outputs fall below
1e-12. A new strike or sustain reactivates it. This removes inaudible filter
work while the physical string keeps resonating. Init explicitly clears the
trigger and excitation flags.

Local explicit release: Adsr::Release sets the release segment
and clears its observed gate. Engine calls it for note-off, scoped mute and
stop, including pulses that begin and end before the first processed sample.
