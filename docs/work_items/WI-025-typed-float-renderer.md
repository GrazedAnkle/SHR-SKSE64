# WI-025: Typed Float Beat Renderer

Status: `[DEFERRED]`

## Outcome and acceptance criteria

Heartbeat DSP operates on a typed float audio buffer with explicit sample rate, channel count, frame count,
and views. A pure beat renderer is callable without Skyrim or XAudio; PCM conversion happens once at the
audio-sink boundary. Stage-level tests and a golden C++/NumPy comparison demonstrate parity for rest, peak,
recovery, inspiration, and PVC fixtures before any deliberate retuning.

## Current conclusion

`HeartbeatVoice.cpp` currently combines WAV parsing, source conditioning, analytic-envelope processing,
beat rendering, repeated PCM16 byte conversion, XAudio voice creation, and submitted-buffer lifetime.
Most DSP functions mutate `std::vector<std::byte>` and use `memcpy` for each sample. This hides frame and
channel invariants, makes intermediate clipping/quantization implicit, and prevents the C++ renderer from
being tested independently of the plugin.

The synthesis docs' source -> transmission -> transducer order is already marked in `HeartbeatVoice::Play`;
the refactor should make those stage boundaries executable without changing their order.

## Scope and non-goals

Introduce a small interleaved float `AudioBuffer`/view, separate source loading and conditioning from pure
per-beat rendering, and leave XAudio in a sink adapter. Do not retune filters, envelopes, amplitudes, sample
landmarks, or limiter behavior. Do not add a general-purpose DSP framework.

## Dependencies

- Coordinate with [WI-011](WI-011-post-exciter-lobe-tail.md) so a mechanical representation change and
  an audible envelope decision are not reviewed in the same comparison.
- [WI-024](WI-024-core-runtime-boundary.md) owns the build/runtime boundary.
- [WI-022](WI-022-audio-resource-ownership.md) owns queue and voice lifetime.

## Next action and decision points

Define the smallest buffer/view contract and add PCM16-to-float and float-to-PCM16 identity fixtures.
Port one source-conditioning stage at a time, comparing each intermediate with
`tools/engine_offline.py`, then extract `BeatRenderer`. The maintainer performs the final level-matched ear
gate even when numeric parity passes.
