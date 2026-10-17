# Optional Android installed-voice PCM provider

`ICE_BUILD_ANDROID_SPEECH=ON` adds `ice::android_speech`, which depends on the common `ice::audio_speech` jobs. It is off by default. The private PCM/segmentation core also builds on desktop for quiet tests. The JNI factory exists only on Android; including its header on a host does not require JNI headers or expose a fake Android factory.

This is a provider of copied PCM, separate from playback, microphones, models, scripts and network transport. It never calls `TextToSpeech.speak`, starts a download/settings activity or creates a playback device. Android's installed engine is an independent service: offline flags are declarations, not a guarantee that the engine cannot access a network. Quest engine/voice availability requires explicit runtime discovery and is not established by cross-compilation.

## Preparation and lifetime

The host packages `engine/java/org/ice/audio/AndroidSpeechBridge.java` (available through the `ICE_JAVA_SOURCES` property of `ICEAndroidSpeech`) and declares the standard visibility query:

```xml
<queries>
    <intent><action android:name="android.intent.action.TTS_SERVICE"/></intent>
</queries>
```

Call `android_speech_preparation::begin(vm, context, options)` explicitly on the intended native owner thread. The borrowed Context is used to load the packaged bridge through its public class loader. Java retains the application Context, not an Activity. The JavaVM must outlive the preparation, provider and any finishing callbacks. Preparation schedules initialization on Android's main looper and returns immediately. `poll()` returns an empty optional while pending, transfers one idle provider when ready, or reports unavailability. It never waits for initialization on the UI thread. Retire a pending preparation when the host shuts down; a stuck external service is not an instruction to block a frame or UI thread.

The generic discovery identity is `android_installed_tts`; voice names/locales are observed from the connected service. `android_speech_ready.requested_engine` retains the original engine request separately. Public `TextToSpeech` constructors permit engine fallback and the actual-engine accessor is not a public SDK method. No hidden API or reflection is used to imply stronger engine provenance.

Discovery excludes voices that advertise network requirements or `KEY_FEATURE_NOT_INSTALLED`. Immediately before `setVoice`, selection refreshes the service inventory and requires the exact unchanged Voice to still advertise installed, nonnetwork operation, then checks the selection and rate result. The external service can change after that observation; this is not an attestation of its behavior. Missing/changed voices fail; no alternative voice is selected by this wrapper. Pass the prepared provider to `speech_jobs::create` with explicit limits. Keep all provider/job operations and destruction on their creating native thread. Callbacks use nonreusing registry IDs and weak ownership rather than native object addresses.

## Synthesis and resources

The API29 route uses `synthesizeToFile` with a fresh owner-private seekable file in the dedicated application cache folder `ice-speech-pcm`. API24 `onBeginSynthesis` and `onAudioAvailable` supply source format and copied PCM. A pipe is not a replacement for this destination: the framework seeks back to write its WAV header. The provider consumes callback PCM directly; it does not decode the temporary WAV. Scratch is distinct from durable microphone recordings and is never exported automatically. Files are removed after each segment or cancellation; a process crash can leave owned temporary cache files. There is no automatic sweep that might delete another live provider's file. Cleanup failure can likewise leave a private cache file.

The native callback layer supports 8000..192000 Hz mono/stereo PCM8 unsigned, PCM16 signed and float32 on the qualified little-endian Android ABI. It normalizes integer samples, rejects nonfinite/out-of-range floats and changing formats, and retains at most seven incomplete frame bytes. Conversion uses a fixed 4096-float scratch buffer; one callback is limited to 64MiB before copying. The common speech mailbox enforces configured copied-byte/chunk/duration limits. These are not a hard kernel quota on an external engine's own scratch or allocations; the wrapper requests stop when delivery fails.

Native text is split below `getMaxSpeechInputLength` into at most 65536 Unicode-safe UTF-16 segments, preserving every character without splitting surrogate pairs. Portable input is bounded by 16MiB, matching the common service's maximum configurable text budget. Segments are queued one at a time and retain one logical generation/PCM format. Only the last complete segment marks synthesis complete. Playback drain remains a separate later event.

Callbacks serialize format, PCM and terminal updates. Cancellation immediately retires the native output generation, then schedules Java stop; late callbacks cannot feed a replacement. Java coalesces pending owner commands into one latest request rather than retaining unbounded posted text arrays. UI/native failures become diagnostics. Destruction requests service shutdown and expires callbacks, without waiting for a service response.

## Qualification boundaries

The portable tests exercise exact Unicode segmentation, PCM8/16/float normalization, arbitrary chunk/frame boundaries, multi-segment completion, format failures, cancellation and a deterministically late callback after replacement. Android consumer tests force the public factory to link but use invalid arguments before starting Java. Java is compiled against SDK34; arm64 Android29 consumers are cross-linked in strict C++20/23 using the existing NDK26d baseline. These are SDK/source proofs, not runtime voice or headset proofs. Ordinary tests do not start an installed engine or physical output.

App Activity/manifest integration, pre-microphone reply cancellation, audible output/audio focus and Quest's OpenAL/newer-NDK qualification remain separate milestones. No device installation or runtime test is implicit in enabling this optional library.

Official contracts: [TextToSpeech initialization and synthesis](https://developer.android.com/reference/android/speech/tts/TextToSpeech), [PCM callbacks](https://developer.android.com/reference/android/speech/tts/UtteranceProgressListener), [voice network metadata](https://developer.android.com/reference/android/speech/tts/Voice), [notInstalled feature](https://developer.android.com/reference/android/speech/tts/TextToSpeech.Engine#KEY_FEATURE_NOT_INSTALLED).
