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

The shared Android host now writes immutable takes through `ice::audio_store` to
`files/recordings/<random-session>-<counter>/capture.wav` and `receipt.json`.
Only an atomic `committed` marker makes a take discoverable as complete. File
contents are flushed before publication; POSIX also flushes the containing
directory. Windows uses flushed files and a write-through marker rename; portable
Win32 does not provide a directory-fsync equivalent, so power-loss guarantees
depend on the filesystem. All clips remain private and retained until an explicit
`remove(id)`. `acknowledge(id)` writes a durable marker without deleting audio or
implying permission to share it. Legacy `voice-0.wav`, `voice-1.wav` and
`voice-latest.json` are neither rewritten nor deleted by the new store.

`recording_store::open(directory, limits)` retains a single native writer lock.
Its parent directory is supplied by a trusted app host. Exclusive private file
creation and directory-relative POSIX access reject symlinks and overwrites; Windows
uses an owner-only DACL and rejects reparse points. The default quota is256MiB and
10000 recording directories. Limits are configurable (bytes1..1TiB, count1..1000000).
Count/byte exhaustion rejects new work, never evicts an older take. Discovery and
explicit removal remain usable after reducing a quota below retained usage.
Cancellation removes only this owner's still-empty reservation. Interrupted writes
remain in `incomplete_ids` and consume quota; explicit removal can recover them.
A removal marker hides a clip before deletion, so a crash during removal cannot
resurrect it as a playable receipt. Cleanup is never implicit on restart.

`list()` is a recovery/discovery operation, not a per-frame query. It validates
committed metadata, exact mono PCM16 WAV layout, duration bounds and WAV content
SHA256; corrupt or foreign data produce a diagnostic rather than silently omitting
a clip. Commit hashes identify content and do not authenticate a speaker or source.
The store is owner-thread only; no background capture, sender, polling process or
filesystem watcher is created. An interrupted final directory flush can leave a
committed recording although commit returned a diagnostic; recovery resolves this
uncertainty. The host never queues a failed/uncertain receipt automatically.

Version2 JSON keeps `recording_id`, `source=aaudio_microphone`,
`consent=user_push_to_talk_X`, `automatic_sharing=false`, `format=pcm_s16le`,
`channels=1`, `sample_rate`, `frames`, `dropped_frames` and `xruns` for transcription
consumers. These provenance strings describe the trusted host's PTT profile; the
portable encoder cannot independently prove consent or acoustic speech. It adds
store-relative paths, WAV byte count/hash, stop reason, and start/end nanoseconds
from `std::chrono::steady_clock`. The interval brackets host stream start/stop,
including driver startup/stop latency; it does not timestamp individual samples or
silently equate host time with OpenXR predicted display time. No raw tracking data
are stored in the audio receipt.

The once-per-frame XR observation appends a sample `monotonic_ns`, take ID/start/end,
voice state/event sequence, bounded diagnostic and optional completed receipt.
The first application update and both eyes observe `began`, then `recording` while
held. The exact terminal host stamp remains available even when focus loss delays the
next observation until resume. A completed receipt or terminal cancellation/failure remains until the next
explicit PTT attempt; clients deduplicate by recording ID. Completion paths are
relative to `internalDataPath` and prefixed `recordings/`. This lets an app explicitly
retain frame input/pointing samples on the same host clock without advancing a
second simulation or claiming a hardware audio/XR time conversion. Permission
failure can have no take ID; event sequence still distinguishes the attempt.

The next voice path should be: explicit completed clip → authorized transport to a
speech recognizer → editable transcript → the same command/session service used by
typed input and agents. Recording alone is neither ASR nor an LLM integration.
Recognition results should carry recording/model provenance and require the same
schema, handle, revision and permission checks as typed requests. Speech/sound
creation needs an explicit playback/output adapter and is not implemented here.

`tests/recording_store.cpp` exercises restart/incomplete recovery, quota rejection,
exclusive/no-overwrite publication, WAV/metadata tampering, durable acknowledgment
and interrupted explicit removal with synthetic PCM only.
`tests/audio_capture.cpp` checks limits, bounded retention/overflow, clearing,
ownership moves and exact WAV byte layout without opening a microphone. Physical
permission/action/acoustic/lifecycle checks are separately recorded in the XR
hardware validation document; build/signing alone does not establish those results.
