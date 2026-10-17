#pragma once
#include "ice/audio/speech.h"
#if defined(__ANDROID__)
#include <jni.h>

namespace ice::audio {
/// Explicit Android provider preparation. Empty requested_engine uses the OS default. Android's
/// public API may fall back to another engine; this value is a request, not verified provenance.
struct android_speech_options {
	/// Optional installed engine package request, valid UTF-8 up to512 bytes. No installation,
	/// voice download, settings UI or network request is initiated by this wrapper.
	std::string requested_engine;
};
/// A freshly prepared, idle provider and its original engine request. Its discovery identity is
/// android_installed_tts with observed voice metadata; no strict engine/network attestation.
struct android_speech_ready {
	/// Owner-thread provider. Only advertised offline voices without notInstalled are exposed.
	std::unique_ptr<speech_provider> provider;
	/// Original engine package request, distinct from the actually connected engine.
	std::string requested_engine;
};
/// Move-only asynchronous initialization over the packaged org.ice.audio.AndroidSpeechBridge.
/// Public operations/destruction are serialized on the creating native thread. Android service
/// initialization is scheduled on its UI thread, never waited for there. The JavaVM must outlive
/// this owner/provider; the Context is retained as an application-context global reference by
/// Java. No speech or physical output starts during preparation.
class android_speech_preparation {
public:
	/// Begin explicit discovery using a borrowed Context valid for this call. The Java bridge must
	/// be packaged and TTS_SERVICE declared in manifest queries. Failure contains JNI exceptions.
	static ice::expected<android_speech_preparation, diagnostic>
	begin(JavaVM *vm, jobject context, android_speech_options options = {});
	/// Cancel unfinished initialization, retire callbacks and request native service shutdown.
	~android_speech_preparation();
	/// Transfer on the same owner thread; the source becomes empty.
	android_speech_preparation(android_speech_preparation &&other) noexcept;
	/// Retire the previous preparation and transfer the new one on its owner thread.
	android_speech_preparation &operator=(android_speech_preparation &&other) noexcept;
	/// Native preparation has one owner.
	android_speech_preparation(const android_speech_preparation &) = delete;
	/// Native preparation cannot be copied.
	android_speech_preparation &operator=(const android_speech_preparation &) = delete;
	/// Nonblocking owner-thread service poll. Empty optional means initialization is pending;
	/// a populated result transfers its idle provider exactly once. Errors never start speech.
	ice::expected<std::optional<android_speech_ready>, diagnostic> poll();

private:
	struct state;
	explicit android_speech_preparation(std::unique_ptr<state> value);
	std::unique_ptr<state> state_;
};
} // namespace ice::audio
#endif
