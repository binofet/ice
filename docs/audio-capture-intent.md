# Explicit microphone intent and reply ordering

`ice::audio::capture_intent` is an owner-thread lifecycle coordinator in `ice::audio`. It binds native preparation, a pre-start observer, native start/stop and an ending observer once. It creates no devices, background work, synthesis or playback. All referenced owners must outlive the coordinator and its destructor; callbacks must not reenter it.

`begin()` runs preparation first. Only after negotiated input and bounded PCM storage exist does it attempt `before_start`, then native start. Rejection or an exception prevents later stages and stops/closes any prepared native resource. Every attempted pre-start observer receives exactly one ending notification after native cleanup is attempted, even when that observer rejected or threw. A permission rejection handled by the host, or a preparation failure before the observer, produces no observer notification. Cleanup failures remain explicit diagnostics; a stop error does not establish that native hardware ceased operating.

`end()` is idempotent, stops input before notification and contains both callback exceptions. The ending callback receives `native_stop_confirmed`, computed before notification: it is false if native stop/close rejected or threw. A presentation owner must keep speech suppressed on false until native uncertainty is explicitly resolved. It clears the lifetime before dispatching cleanup. Native completion, user release, focus/action/permission loss, failed startup and teardown use the same retirement path. The destructor also retires an open lifetime, but hosts should call `end()` explicitly to inspect cleanup errors.

## Android/OpenXR boundary

The existing `android_capture::start(duration)` convenience call still prepares and starts input. The separate `prepare(duration)` and `start_prepared()` operations let the XR host insert a synchronous observer after allocation and before `AAudioStream_requestStart`. Preparing opens a negotiated input stream but does not start recording. Stop/discard also closes a prepared stream. A native cleanup failure remains an error across repeated stop/discard and blocks new preparation on that capture owner; retirement/recreation requires explicitly resolving the native input condition first.

The optional `android_client::before_microphone_start()` returns an audio diagnostic on failure; `after_microphone_stop(bool native_stop_confirmed)` balances that attempt. Both run on the existing native owner thread. Default implementations preserve clients without reply audio. The host invokes the hook only after explicit, permission-checked PTT intent and private recording reservation. It does not synthesize a capture gesture, grant permission, change microphone storage or activate reply playback. Stream-start timing remains a host steady-clock bracket, not a hardware acoustic timestamp.

An application with reply audio must synchronously retire synthesis generations and stop its playback stream in the pre-start observer. Setting a speaker's recording flag from the later `frame_observation` is insufficient: native microphone startup occurs before that frame reaches the application. Ending the microphone lifetime only removes suppression; it must not replay cancelled or previously skipped narration. Driver/output buffers and external services still prevent an absolute acoustic silence guarantee.

## Fresh reply delivery

Drain typed completion FIFOs on every service turn while disabled, muted, inactive or recording, consuming those delivery identities or explicitly counting discarded events. Immediately before enable, unmute or resume, drain pending source FIFOs while the **old suppressed state** still applies; then change state. Otherwise a previously accepted completion can become newly audible solely because a setting changed. Console refresh, journal recovery and receipt retry are not fresh completion events.

Service polling remains separate from XR rendering and simulation. It can drain receipts and heartbeats while rendering is idle, subject to Android process scheduling. It must not start capture, replay output or mutate a suspended world. Pump presentation work once per owner-thread turn, never once per eye. This slice adds no app speech activation, physical output or background-service guarantee.

## Qualification

Portable fake-driver/provider tests use real bounded PCM allocation and the production coordinator to assert cancellation-before-start, stale synthesis rejection, preparation/start/observer failures, native cleanup ordering, balanced notifications with true/false native cleanup results and idempotent teardown. Android arm64/API29 builds check the actual AAudio split and XR host linkage in C++20/23. Device microphone/reply acoustics and application opt-in integration require separate runtime qualification.
