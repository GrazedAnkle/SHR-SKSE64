# WI-022: Audio Resource Ownership

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Every XAudio source voice and submitted beat buffer has one explicit owner on success, failure, flush, and
shutdown. `SubmitSourceBuffer` failure cannot leak a buffer, voice destruction is automatic, and tests or
a narrow fake-sink fixture cover failed submission and queued-buffer release.

## Current conclusion

`HeartbeatVoice::Play` takes the core-rendered float beat, encodes it once to PCM16 at the sink boundary,
allocates that sample vector with `new`, passes it through the XAudio context pointer, and relies on
`OnBufferEnd` to delete it. The return value from `SubmitSourceBuffer` is ignored, so a rejected buffer
has no callback-owned cleanup path.
`HeartbeatVoice::Shutdown` destroys the raw voice but has no caller, leaving lifetime dependent on
process/static teardown rather than the class.

## Scope and non-goals

Define RAII ownership for the voice and an exact transfer protocol for queued buffers. Preserve rendered
samples, playback order, pause semantics, the one-time PCM16 sink conversion, and the current direct
mastering-voice route. Do not move device or queue ownership into `shr_core`.

## Dependencies

None. The established sink boundary keeps rendered float audio caller-owned and transfers encoded storage
to callback ownership only after successful submission.

## Next action and decision points

Confirm XAudio callback behavior for flush and failed submission, then wrap the voice and submitted buffer
context in types whose destructors encode the result. The maintainer decides whether a small reusable
buffer pool earns its complexity; it is not required for correctness.
