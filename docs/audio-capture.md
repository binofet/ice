# Bounded push-to-talk capture

`ice::audio` provides portable mono signed PCM16 storage and RIFF/WAVE encoding.
`capture_buffer::create(rate, duration_ms)` validates 8000..192000 Hz and 1..10000 ms,
allocates the entire buffer once, and rounds its duration down to whole frames.
`append` accepts the prefix that fits, counts rejected frames with saturation and
never allocates or overwrites earlier audio. `reset` zeroes its retained storage.
Moves transfer ownership and leave the source empty with sample rate zero.

The owner serializes append/read/reset/move/destruction. Borrowed sample spans do
not outlive their owner or a reset/move. `encode_wav` returns owned little-endian
PCM16 mono bytes with an ordinary 44-byte header; it performs no file or network I/O.
The largest retained clip is 1,920,000 frames / 3,840,000 PCM bytes. Encoding can
temporarily allocate another clip-sized block. These are allocation requests,
not a process RSS guarantee. Allocation failures are diagnosed where possible;
constructing an error can itself fail under process-wide exhaustion.

## Android input

On Android, `ice/audio/android_capture.h` adds an owner-thread AAudio input adapter.
Constructing it opens nothing. The host first obtains `RECORD_AUDIO` permission
and explicit foreground recording intent, then calls `start`. Android also checks
permission while opening input. The adapter requests shared mono PCM16 with the
voice-recognition preset, accepts the negotiated sample rate only within the
portable range and exposes that actual rate on its retained buffer. It does not
silently relabel/resample audio or accept a different sample format/channel count.

`poll` uses zero-timeout reads, at most eight 2048-frame chunks per call. It handles
asynchronous start without blocking and stops/closes on full storage, the next
poll after the requested elapsed duration, or a stream/read failure. There is no
automatic retry/restart. No callback thread writes caller storage; reads, stream
state, stopping and closing all belong to the same host thread. See Android's
[AAudio ownership/read guidance](https://developer.android.com/ndk/guides/audio/aaudio/aaudio).

Call `stop` on release, focus/pause, permission loss and shutdown. It closes the
stream without reading more queued audio and retains completed PCM for the host.
`discard` also zeroes/destroys the retained buffer; call `stop` first when its
native error must be observed. Unread device tail frames are omitted, never padded
with invented samples. Polling too slowly can lose input; the last supported AAudio
xrun count is exposed, with unavailable counts reported as zero. No count proves
speech, acoustic quality or absence of all OS losses. A native close can block;
the owner must continue polling for the elapsed limit to take effect. This is not
a hard wall-clock watchdog or a low-latency production audio mixer.

There is no always-on/background service, permission bypass, mic device identifier,
network transport or automatic external upload. Other native capture backends,
playback/synthesis, mixing, resampling and speech recognition remain future modules.
`cmake/ICEAudio.cmake` has no third-party dependency; only Android links `aaudio`.
The root optional `ICE_BUILD_AUDIO` selection does not initialize hardware.

## Quest probe and provenance

ICE XR Lab uses **left-controller X held** as PTT. The first press requests the
standard [Android runtime permission](https://developer.android.com/training/permissions/requesting)
on the UI thread. Granting permission does not record: release X and press it
again. Red on the small bar below the rotating cube and the left grip means an
input stream is active; amber means permission is needed; green means a clip was
saved. A mic privacy toggle can yield silent audio even with permission granted.

Release finalizes a clip; the duration/frame cap finalizes without needing a
release, then requires release before another recording. Android pause/lost focus,
OpenXR focus loss, inactive PTT action, revoked permission and shutdown stop and
discard unfinished PCM. Resume cannot restart it. Existing head/controller tracking,
grip/trigger actions and room-origin correction remain independent of capture.

The example writes only app-private `files/voice-0.wav`, `voice-1.wav` and
`voice-latest.json`. Two fixed slots bound retained disk usage across launches;
one inactive-slot temporary file is possible while saving. A completed inactive
WAV is renamed first, then the small latest metadata pointer is atomically renamed.
Readers begin with that metadata and fetch its named WAV; concurrent readers must
copy promptly before two later recordings reuse the same slot. This is an example
retention policy, not a shared cross-process recording database.

Metadata records ephemeral session/recording IDs, user-PTT consent, source/build,
start wall time, sample rate, mono PCM format, frames, rejected frames, observed
xruns, rounded PCM peak/RMS and stop reason. It contains no microphone serial or
tracking positions. Logs contain these diagnostics, not raw sound or transcripts.
The recording identity is the pair `(session, recording_id)`; an Activity restart
can reuse its per-instance sequence while the Android process remains alive.
There is no automatic sharing. An authorized developer can explicitly retrieve the
latest manifest and selected WAV with this debug app's `run-as` identity.

The next voice path should be: explicit completed clip → authorized transport to a
speech recognizer → editable transcript → the same command/session service used by
typed input and agents. Recording alone is neither ASR nor an LLM integration.
Recognition results should carry recording/model provenance and require the same
schema, handle, revision and permission checks as typed requests. Speech/sound
creation needs an explicit playback/output adapter and is not implemented here.

`tests/audio_capture.cpp` checks limits, bounded retention/overflow, clearing,
ownership moves and exact WAV byte layout without opening a microphone. Physical
permission/action/acoustic/lifecycle checks are separately recorded in the XR
hardware validation document; build/signing alone does not establish those results.
