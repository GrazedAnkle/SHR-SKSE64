# WI-026: Runtime Thread Contract

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Every mutation of runtime, simulation, rhythm, and audio-control state has a documented execution thread
or synchronization mechanism. Cross-thread SKSE callbacks, if any, enqueue typed events for one update
thread to consume; otherwise verified same-thread assumptions are encoded and unnecessary atomics are
removed. A stress or deterministic mailbox test covers concurrent event delivery where required.

## Current conclusion

`Runtime` forwards typed notifications directly to `HeartRateSimulation`, which uses atomics for jump,
sleep, and fast-travel notifications, while combat and hit callbacks mutate `m_Adrenaline` directly and
the remaining model state is ordinary scalar state. `InputHandler` separately exposes a reference to an
`atomic_int`, and XAudio releases buffers from its callback thread. This mixture does not state whether
SKSE event sinks and the player update hook are guaranteed to share a thread. It is therefore an
architectural uncertainty and a possible data-race risk, not a confirmed defect until the engine callback
contract is established.

## Scope and non-goals

Establish and encode the actual threading contract. Prefer single-writer model state with a small event
mailbox if callbacks can cross threads. Do not add locks inside numerical update functions without a
demonstrated producer/consumer requirement, and do not redesign XAudio's callback contract beyond the
ownership work in [WI-022](WI-022-audio-resource-ownership.md).

## Dependencies

Use the established [`Runtime` event-ingress boundary](../ARCHITECTURE.md#runtime-contract).

## Next action and decision points

Verify the documented or observed delivery threads for each registered SKSE event sink, input events, the
player update hook, serialization callbacks, and XAudio buffer callbacks. Record the matrix before choosing
between direct forwarding and a mailbox. The maintainer decides whether unsupported engine versions need a
defensive mailbox despite same-thread behavior on the supported runtime.
