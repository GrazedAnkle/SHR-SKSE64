# WI-026: Runtime Thread Contract

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Every mutation of runtime, simulation, rhythm, and audio-control state has a documented execution thread
or synchronization mechanism. Cross-thread SKSE callbacks, if any, enqueue typed events for one update
thread to consume; otherwise verified same-thread assumptions are encoded and unnecessary atomics are
removed. A stress or deterministic mailbox test covers concurrent event delivery where required.

The adapter's file-scope mutable state is consolidated into one named owner with a single lifetime, so
the threading question is answered once rather than six times.

## Current conclusion

`Runtime` forwards typed notifications directly to `HeartRateSimulation`, which uses atomics for jump,
sleep, and fast-travel notifications, while combat and hit callbacks mutate `m_Adrenaline` directly and
the remaining model state is ordinary scalar state. `InputHandler` separately exposes a reference to an
`atomic_int`, and XAudio releases buffers from its callback thread. This mixture does not state whether
SKSE event sinks and the player update hook are guaranteed to share a thread. It is therefore an
architectural uncertainty and a possible data-race risk, not a confirmed defect until the engine callback
contract is established.

The adapter's own state shape makes this harder to answer than it needs to be. `SkyrimHeartRate.cpp` keeps
four independent mutable file-scope objects - the runtime, the heartbeat voice, the last game-hours sample,
and the previous heart-rate level - alongside the process-global configuration and the input handler's
listening flag. Each has its own implicit lifetime and its own implicit writer. Determining the threading
contract means answering the question separately for six things rather than once for one owner.

That consolidation is no longer only a convenience. [WI-034](WI-034-mcm-capability.md) has to deliver
validated settings updates into *something*, and the object it delivers into is exactly this owner: a
settings change touches the runtime, the voice's volume, and adapter-level configuration together, which
is incoherent to express against six independently-scoped variables. Consolidating first is therefore a
prerequisite of that item rather than a cleanup that could follow it.

The consolidation also closes a standing gap that has nothing to do with threading. The heart-rate-level
change detection decides *when* to consult `NotificationPolicy`, which owns only message selection. That
edge detection is untested logic living in the one file nothing can test, because its state is a file-scope
variable in a translation unit that pulls in the game runtime. Once the previous level belongs to a named
owner, a small level tracker becomes ordinary testable code.

## Scope and non-goals

Establish and encode the actual threading contract, and consolidate the adapter's file-scope state into a
single owner with an explicit lifetime and a stated writer. Prefer single-writer model state with a small
event mailbox if callbacks can cross threads. Do not add locks inside numerical update functions without a
demonstrated producer/consumer requirement, and do not redesign XAudio's callback contract beyond the
ownership work in [WI-022](WI-022-audio-resource-ownership.md).

## Dependencies

Use the established [`Runtime` event-ingress boundary](../ARCHITECTURE.md#runtime-contract).

[WI-034](WI-034-mcm-capability.md)'s feasibility spike observes which thread a menu-driven setting change
is delivered on. That is one more row of the same matrix, so run the spike before the matrix is declared
complete rather than discovering the row afterwards.

## Next action and decision points

Verify the documented or observed delivery threads for each registered SKSE event sink, input events, the
player update hook, serialization callbacks, XAudio buffer callbacks, and menu-driven settings changes.
Record the matrix before choosing between direct forwarding and a mailbox. The maintainer decides whether
unsupported engine versions need a defensive mailbox despite same-thread behavior on the supported
runtime.
