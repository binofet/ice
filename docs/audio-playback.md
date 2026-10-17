# PCM streaming and spatial playback

`ice::audio_playback` is an optional, owner-thread output service backed by pinned
OpenAL Soft 1.25.2. Enable `ICE_BUILD_AUDIO_PLAYBACK=ON`; it defaults OFF while native
platform qualification is incomplete. Enabling the module initializes no device.
Existing AAudio capture and the durable recording store remain independent.

`playback_device::create` explicitly selects a physical default output or genuine
offline loopback mixer. Native output rejects the null/file backends; loopback
opens no audio hardware and writes interleaved stereo float samples into caller
storage. Capabilities report the actual renderer, output rate, EFX availability,
HRTF extension and active HRTF state. `prefer` permits an explicit HRTF fallback;
`require` fails if activation cannot be obtained. Extension presence alone never
establishes active HRTF or successful hardware output.

## Owned queued streams

Create a stopped voice with a fixed mono/stereo format and upload normalized,
finite float PCM using `queue`. The backend copies samples before returning, so a
TTS provider or decoder can release its source block immediately. `play` expresses
play intent; empty unfinished queues become `starved`. Later blocks restart only
an explicitly playing, nonpaused voice when serviced by `poll`/`inspect`/loopback
`render`. `finish` marks a final block sequence; draining becomes `completed`.
`stop` drops queued PCM and resets end/starvation state. `retire` invalidates the
handle generation; foreign, stale and moved-from owners reject operations.
Completion describes the source queue; shared reverb/filter tails may outlast its
last source sample.

All service calls, moves and destruction must be serialized on the creating
thread. Per-thread native context scopes restore the previous thread context;
they do not install a process-global context. No application, Lua, decoder, file,
network or provider callback runs on the native mixer thread. Upload and native
resource allocation are synchronous owner-thread work, so decode/prepare ahead
of a latency-sensitive frame. This wrapper is not a hard real-time scheduler.

Default live budgets are 1024 voices, 512 MiB of uploaded float PCM and 65536 queued
blocks. The wrapper accepts 1..65536 voices; native source limits can be lower.
Budgets are configurable. Exhaustion
rejects new work without stealing a voice or evicting queued audio. Process RSS
can exceed PCM accounting because native mixer, filter, queue and library storage
are additional. Budget accounting is reclaimed by servicing processed buffers;
hosts should keep pumping while active. Frame counts include the partially played
native block and are not a sample-accurate acoustic presentation cursor.

`suspend` pauses output while preserving each voice's play/pause intent and PCM;
`resume` is explicit. A detected disconnected output rejects further operations
until the host recreates the device and its desired streams. There is no automatic
reopen, background playback entitlement or OS interruption observer. Mobile hosts
must connect foreground/audio-session/interruption lifecycle to these methods.
When a native operation fails after previous setters succeeded, its diagnostic
does not claim universal rollback of native state. Validation and queue-budget
rejections occur before publishing a new block. Under process-wide allocation
failure, constructing an owned diagnostic can itself fail.

## Spatial profile and effects

Spatial sources are mono point emitters in ICE's right-handed metre space,
with +Y up and -Z forward. Stereo is explicitly nonspatial. Positions/velocities
are finite within±1000000 per coordinate; listener forward/up are finite,
noncollinear and normalized/orthogonalized before native setters. Gain is0..16,
pitch is greater than 0 and at most 16, reference distance is greater than 0 and at
most 1000000, and inverse-distance rolloff is 0..1000. These are validation ranges,
not recommended artistic settings. OpenAL handles mixing, resampling, attenuation,
panning and Doppler from source/listener velocity.

The initial EFX profile exposes per-voice low-pass high-frequency gain and a send
to one shared native default reverb. Both controls are 0..1. Neutral defaults work
without EFX; requesting unavailable effects returns a diagnostic. There is no
scene-geometry occlusion, convolution, reflection tracing, authorable effects graph
or emitter component binding yet. Head-tracked spatial output requires the host
to publish its actual listener pose; merely creating a voice does not do that.

## Decoder boundary

`ice::audio_pcm` is a separate CPU library (`cmake/ICEAudioPCM.cmake` can be consumed
without OpenAL). `decode_wav` accepts owned RIFF/WAVE bytes and returns retained
PCM without file/network/device access. The first profile supports mono/stereo
PCM16/24/32 and IEEE float32, 8000..192000Hz, ancillary chunks and odd-byte padding.
It validates RIFF length, format/data uniqueness, frame alignment, byte rate,
normalized finite samples and configured encoded/decoded budgets (512MiB each by
default). It rejects WAVE extensible/compressed formats and performs no implicit
resampling. Empty valid PCM is accepted; an empty playback queue block is not.

FLAC, MP3, Vorbis and Opus are future decoder plugins with separate source/license
and streaming tests. They are not implemented by this module. Queued PCM already
lets any separately authorized speech synthesizer feed output without coupling
the engine to a provider, language model or codec. AI spoken replies still require
application integration, explicit output policy and a synthesizer.

## Dependency, mobile builds and distribution

The source archive is the official
[OpenAL Soft 1.25.2 release](https://openal-soft.org/openal-releases/openal-soft-1.25.2.tar.bz2),
SHA256 `1dbaac44e7579d5bc8847ca8db4b2e8b9fd3961041f35ee20def4958301e1089`.
`ICE_OPENAL_SOURCE_DIR` is an explicit developer-supplied source override; its
version is checked, but callers must verify the override's source integrity.
No checked-in binary or system OpenAL implementation is substituted.

Development linkage is static PIC. Apple requires CoreAudio, Windows requires
WASAPI, and Android requires OpenSL ES with Oboe disabled. Oboe/AAudio output is a
future backend choice; the existing AAudio microphone is unchanged. Linux native
backends depend on installed development libraries. Loopback remains distinct
from native output. Watch output, device interruption recovery and shipping
configuration are unqualified until separately exercised. Simulator/SDK link
proof is not evidence of acoustic playback on an iPhone, Quest or other device.

Android NDK 26d's libc++ has headers without the `std::format` and
`lexicographical_compare_three_way` implementations used by this upstream release.
Playback configuration diagnoses that incomplete C++20 library early. The isolated
consumer is qualified with official NDK r30 (`30.0.16248370`), arm64 and API29 in
both language modes. Its ELF Android identification retains API29. The official
macOS archive checksum is SHA1 `c060be96767eefbb8e0a27796d6f43115fc1a0c4`, verified
against [Google's package metadata](https://dl.google.com/android/repository/repository2-3.xml).
Use one coherent NDK/STL for the eventual app; this does not qualify mixing C++
objects/runtimes from different NDKs. The existing Quest NDK 26d build is unchanged;
adopting a new toolchain for that complete application is a separate gate.

The pinned CoreAudio source predates Xcode 17's SDK 26.2 `AURenderCallback`
`nonblocking` function-effect declaration. For AppleClang 17+, only that upstream
translation unit demotes `-Wfunction-effects` from error to a retained warning,
after compiler-flag discovery. ICE warnings remain errors. No unchecked native
DSP callback is annotated with a guarantee it has not been audited to meet.

OpenAL Soft's release `COPYING` is LGPL2-or-later. Static development linkage
does not waive distribution requirements: a release needs its notices/license,
corresponding library source and an applicable mechanism/materials allowing users
to modify/relink the library, plus the relevant third-party notices. The exact
release also carries `gsl/LICENSE`, `fmt-11.2.0/LICENSE` and `LICENSE-pffft`.
Select a reviewed shipping/relink route before distributing a proprietary/static
mobile executable; this work does not establish App Store eligibility.

The embedded HRTF data in this exact release are MIT KEMAR, per its `docs/hrtf.txt`.
The [original dataset notice](https://sound.media.mit.edu/resources/KEMAR.html)
credits Bill Gardner and Keith Martin and requires citing the authors for
research/commercial use. Current upstream master documentation may describe a
different dataset/license and must not replace this release's provenance.

## Validation

`tests/audio_wav.cpp` uses independent reference PCM16/24/32/float bytes and malformed
RIFF cases. `tests/audio_playback.cpp` opens only genuine loopback contexts and
measures resulting samples: left/right panning, distance attenuation, low-pass
energy reduction, reverb tail and required active HRTF. It also proves owned
upload lifetime, starvation/refill, finish/drain, queue budgets, device/voice pause,
foreign/stale generations, move ownership and wrong-thread rejection.

macOS optimized C++20 and C++23 runs pass both executables, observing OpenAL Soft
1.25.2, 48000 Hz float stereo, EFX/HRTF availability and required active HRTF. The
test prints the actual selected dataset name. Physical output was never opened.
Unsigned iOS arm64 simulator/device C++23 consumers and Android arm64/API29
C++20/C++23 consumers compile and link; no device playback is claimed.

The bounded macOS Debug ASan/UBSan fixtures timed out during sanitizer startup
before test output, with `/bin/ps: Operation not permitted`; their owned processes
were terminated. That is an unexecuted sanitizer gate, not a test pass. Linux,
Windows, native output/interruption and shipping gates remain separately required.
Instrumentation applies to adopted wrapper/decoder targets, not every upstream
DSP translation unit.
