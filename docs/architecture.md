# Architecture

## Ownership and input

The UI thread owns InputDevice, Controller and Settings. A bounded single-producer/single-consumer Audio queue transfers complete settings and event packets to SDL's callback. Studio and Looper belong to that callback. Queue overflow, focus loss and controller removal stop transport and release voices while retaining the score. InputGuard waits for neutral controls before resuming input.

Controller applies shared face-button mapping, pitch modifiers and Meta commands. Command buttons remain consumed until release. Each instrument produces performance events through the same interface. LoopView exposes the authoritative musical position, recording state, capacity and operation feedback to the UI under SDL's audio-device lock.

## Sound catalog and models

The immutable catalog in `sound_catalog.cpp` owns stable IDs, saved keys, display names, instrument families, defaults and model parameters. Navigation, settings and presentation reference this catalog. Add a data row to extend an existing model; a new synthesis method belongs in its own model module.

Studio stamps each attack with its selected sound. Engine returns the actual identity of a held note so recording reflects what was performed. MelodyVoice holds a const preset reference for each attack. Sustained harmonics, struck partials and FM use generic model parameters. Controller emits Keys attacks on button edges and captures the Expressive transpose at gate onset. Guitar bindings live in `guitar_controls.hpp`, shared by input and UI. The Guitar controller builds manual picks and strums through one pluck path. Each button press triggers one string; each stroke schedules six pitches at once. A simultaneous manual pick takes precedence on its string. Pitch modifiers apply to new attacks. `pitch_gesture.hpp` defines the shared 30 ms completion window and octave bounds. A runtime gesture identity scopes late modifier presses to the recent attack, including scheduled strings; audio applies the change directly and the score stores its final pitch. Releasing modifiers and presses outside the window preserve held voices and release tails. Completions across a loop boundary refresh the committed score and invalidate its older cache prefix. Expressive stick movement preserves continuous transitions within the captured register. Bend and vibrato remain independent expression controls.

Guitar uses the DaisySP physical-string model. Drum Machine plays immutable electronic one-shots generated before audio starts. Closed hi-hat chokes open hi-hat. A single output limiter bounds the stereo mix; volume applies at the shared output stage.

## Score, timing and caches

Looper's event score is the authority for pitch, duration, velocity, sound identity, strum timing, expression and scoped mute. A sample-driven clock controls all tracks. Guitar and melody tracks support four layers, defined by `loopLayerCount`. Musical subdivisions and file format identity are centralized in `loop_snapshot.hpp`.

Committed tracks play from stereo PCM caches. Two workers render affected tracks from immutable score snapshots. Each track has three slots with atomic ownership states. Active and fading slots remain immutable. The callback copies snapshots into available slots and reads PCM; workers allocate buffers and synthesize them. Obsolete jobs cancel by generation.

An appended take uses the existing cache plus a delta engine for new material until its replacement is ready. Tempo and length changes prepare new buffers while the current backing continues, then adopt the new configuration together at a loop boundary. Live sound selection leaves committed score revisions and caches intact.

Cache rendering primes two uninterrupted cycles, retains the third and generates a natural continuation. A 64-frame seam blend and replacement fade preserve continuity. Drum tails, guitar decay and note releases continue across wraps. At 120 BPM and one bar, a stereo cache version uses approximately 0.77 MB per track. The largest three-slot, four-track allocation at 40 BPM and two bars is approximately 55 MB.

## Persistence and presentation

Settings save stable sound keys. Current performances use the version-2 schema and `settings.cfg.performance`. Validation checks bounds, event kinds, ordering, finite values and instrument-specific identities before replacing a score. Atomic writes use a temporary file, flush, fsync and rename. Reopened performances rebuild PCM caches and start paused.

`loop_presentation` formats status and contextual actions. `ui_controls` draws common input hints, while `loop_ui` draws musical position, track cards and the drum grid. `ui_draw` owns common drawing primitives. Product identity comes from `product.json`; the build and packagers generate the matching application, launcher and metadata names. Audio exports share WaveFile.

## Validation

Behavior tests exercise public input, musical output, state and persistence. Sound tests cover catalog identity, navigation, natural decay, held-note continuity, per-note recorded sounds, cached playback and attack-time pitch selection. Pitch regressions compare rendered audio with a reference continuation, verify independent chord registers, Expressive wheel movement, scheduled strums and recorded pitches.

ASan/UBSan check memory behavior and TSan checks cache ownership. The optional performance target covers full cached tracks with live guitar, keyboard, expressive performance and simultaneous overdub. Device verification uses the shared executable list in `tests/device-tests.txt`. See [validation summary](验收报告.md) and the documented DaisySP adaptations in `vendor/DaisySP/UPSTREAM.md`.
