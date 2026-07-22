# WI-022: Audio Resource Ownership

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Every XAudio source voice and submitted beat buffer has one explicit owner on success, failure, flush, and
shutdown. `SubmitSourceBuffer` failure cannot leak a buffer, voice destruction is automatic, and tests or
a narrow fake-sink fixture cover failed submission and queued-buffer release.

## Current conclusion

`HeartbeatVoice::Play` allocates a `std::vector<std::byte>` with `new`, passes it through the XAudio
context pointer, and relies on `OnBufferEnd` to delete it. The return value from `SubmitSourceBuffer` is
ignored, so a rejected buffer has no callback-owned cleanup path. `HeartbeatVoice::Shutdown` destroys the
raw voice but has no caller, leaving lifetime dependent on process/static teardown rather than the class.

## Scope and non-goals

Define RAII ownership for the voice and an exact transfer protocol for queued buffers. Preserve rendered
samples, playback order, pause semantics, and the current direct mastering-voice route. The typed float
renderer belongs to [WI-025](WI-025-typed-float-renderer.md), although both items may share a final audio
sink boundary.

## Dependencies

None for failed-submission cleanup. Coordinate the final sink interface with
[WI-025](WI-025-typed-float-renderer.md).

## Next action and decision points

Confirm XAudio callback behavior for flush and failed submission, then wrap the voice and submitted buffer
context in types whose destructors encode the result. The maintainer decides whether a small reusable
buffer pool earns its complexity; it is not required for correctness.
