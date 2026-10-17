# Speech generation into owned PCM

`ICE_BUILD_AUDIO_SPEECH=ON` enables `ice::audio_speech`, independently of capture,
the recording store and OpenAL playback. It defaults OFF during platform
qualification. The portable service coordinates a caller-supplied provider;
Apple builds also supply an installed-system-voice adapter. Other platforms
return an explicit unavailable result from the Apple factory. No application
speech activation, Android TTS engine, external synthesizer or LLM is implicit.

## Job and callback ownership

`speech_jobs` owns one provider and one active utterance. Use separate instances
for simultaneous speakers. Discovery returns provider identity, explicit voice
IDs/names/locales and the allowed rate multiplier range. Each request supplies a
correlation ID, plain UTF-8 text, one discovered voice ID and a rate. The wrapper
does not interpret scripts or SSML. Invalid text/voice/rate, an active job or
undrained PCM rejects before the provider is invoked.

The provider receives a copyable weak `speech_output` token. Its thread-safe
`append`, `complete`, `fail` and `active` methods contain exceptions; they only
validate/copy into a bounded mutex-protected mailbox. They never operate OpenAL,
an application, a scene, a device microphone or a user callback. Each accepted
chunk owns interleaved normalized float samples. The first accepted chunk fixes
the source frequency and mono/stereo format; later changes or nonfinite/out-of-
range samples fail the current job.

The creating thread serializes service operations, provider start/cancel and
destruction. `poll(handle)` returns copied metadata plus owned PCM chunks exactly
once; it performs fallible result allocations before consuming the mailbox.
The callback thread may continue producing while the owner processes previous
chunks. Owner moves preserve handles; foreign/stale handles reject. A native
callback can outlive cancellation or owner destruction because its token is weak
and generation-local. It cannot publish into a replacement job.

`completed` means the provider has finished synthesis. It can still have buffered
PCM and is distinct from drained output or acoustic playback. A new job replaces
only a terminal/drained prior job. Explicit cancellation discards pending PCM and
is idempotent for the current generation; already drained completion and failure
retain their terminal outcome. Failure also discards pending data and owner poll
requests native cancellation. Previously delivered or played audio cannot be
rolled back. The host must explicitly stop/retire its playback voice on failure,
cancellation or lifecycle policy changes. Failed starts cancel their candidate
and preserve the previous terminal/drained job metadata; they are not retried.

## Configurable resource profile

Default limits are 1 MiB of request UTF-8, 128 MiB pending float PCM, 16384 pending
chunks and 3600 seconds of accepted output frames. Text is configurable from
1 byte to 16 MiB, pending PCM from 4 bytes to 1 GiB, chunks from 1 to 1000000 and
duration from 1 to 86400 seconds. Correlation IDs are nonempty UTF-8 up to 256
bytes, voice IDs up to 512 bytes; neither becomes a filesystem path. PCM supports
8000..192000 Hz and one/two channels. The service does not resample.

Duration accounting includes accepted frames already drained by the owner. It is
not a wall-clock watchdog: a stalled provider still needs host cancellation or
an application deadline. Pending-budget exhaustion fails rather than silently
dropping audio or blocking indefinitely for consumption. Native provider scratch,
voice assets and OS worker memory are outside the copied-PCM budget. The callback
mailbox is not a hard real-time mixer interface. Allocation failures are diagnosed
where possible; creating an owned diagnostic can itself fail under process-wide
exhaustion.

## Apple provider

`create_apple_speech_provider` must run on the main owner thread with a serviced
native event loop. It restricts discovery to `com.apple.*` IDs that can currently
be resolved by `voiceWithIdentifier`, and excludes the Personal Voice trait.
Before starting, it resolves the requested installed ID again. Missing assets
return a diagnostic rather than requesting a download or substituting a voice.
It never requests Personal Voice authorization or selects third-party voice IDs.
Recreate the provider to refresh its installed-voice snapshot.

The adapter uses Apple's
[`write(_:toBufferCallback:)`](https://developer.apple.com/documentation/avfaudio/avspeechsynthesizer/write%28_%3Atobuffercallback%3A%29)
to generate audio buffers, and never calls `speakUtterance`. It checks native
buffer sizes/layout, source rate and mono/stereo channels, then converts planar
or interleaved float32/float64/PCM16/PCM32 into fixed-size float scratch chunks.
It rejects invalid/non-normalized values and publishes through the same portable
mailbox. A zero-frame PCM callback marks completion. Cancellation uses the native
stop method; late callback tokens remain inert. No speaker output, audio-session
activation, file publication, network provider or asset installation is performed
by this adapter.

Synthesis and playback are separate services. A host can feed the returned PCM
to `playback_device::queue` with the actual format, explicitly request playback,
and mark the final queue using `finish` after provider completion. Native app
foreground/interruption handling, PTT ducking, user mute/stop controls, final
assistant reply receipts and replay policy belong to application integration.
They are not established by a PCM-generation test. Likewise, Android can have no
installed TTS engine; an Android API build alone would not prove availability.

## Validation and explicit native qualification

`ice_audio_speech` is a portable CTest using a fake provider, with real worker-
thread callbacks. It proves owned PCM lifetime, format/budget failure, provider
exceptions, completion versus drain, cancellation/replacement races, late
callbacks, stale/foreign handles, moves and creator-thread enforcement. No native
voice discovery or synthesis occurs in this ordinary regression.

On macOS with both speech and playback enabled, the separate manual executable is:

```sh
build/audio-playback23/ice_audio_speech_apple_tests --qualify-installed-voice
```

It is deliberately not registered with CTest. Omitting the explicit argument
performs no synthesis. Missing installed voices, cancellation or timeout fails
qualification; there is no skip-as-pass or automatic download. It generates PCM
from a fixed test phrase, queues it into actual OpenAL loopback, checks finite
nonzero output and verifies immediate cancellation of a second generation.

The macOS C++20/23 native-service runs selected installed compact/super-compact
Samantha and produced 70070/72374 mono frames at 22050 Hz, with measured loopback
energy 3089.93/3193.86. They opened no speaker device. A prior sandboxed run discovered voices but
the OS cancelled before delivering PCM; that attempt failed. Native-service
permission and voice availability are qualification preconditions, not inferred
from SDK compilation or portable test success.

The C++23 provider consumer also compiles and links for arm64 iOS Simulator and
unsigned iPhoneOS. The portable coordinator and explicit unavailable Apple
factory compile and link with NDK26 at Android API29 in C++20/23. No mobile native
voice synthesis or speaker output has been qualified by those SDK checks.
