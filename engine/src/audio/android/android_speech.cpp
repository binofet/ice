#include "ice/audio/android/speech.h"
#include "ice/core/utf8.h"
#include "stream.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>

namespace ice::audio {
namespace {
class environment {
public:
	explicit environment(JavaVM *vm) : vm_(vm) {
		if (!vm_)
			throw std::runtime_error("Android speech requires a live JavaVM");
		const auto result = vm_->GetEnv(reinterpret_cast<void **>(&env_), JNI_VERSION_1_6);
		if (result == JNI_EDETACHED) {
			if (vm_->AttachCurrentThread(&env_, nullptr) != JNI_OK)
				throw std::runtime_error("Android speech JNI attachment failed");
			attached_ = true;
		} else if (result != JNI_OK)
			throw std::runtime_error("Android speech JNI environment unavailable");
		if (env_->PushLocalFrame(32) != JNI_OK) {
			if (env_->ExceptionCheck())
				env_->ExceptionClear();
			if (attached_)
				vm_->DetachCurrentThread();
			throw std::runtime_error("Android speech JNI local frame allocation failed");
		}
	}
	~environment() {
		env_->PopLocalFrame(nullptr);
		if (attached_)
			vm_->DetachCurrentThread();
	}
	JNIEnv *get() const noexcept { return env_; }

private:
	JavaVM *vm_;
	JNIEnv *env_      = nullptr;
	bool    attached_ = false;
};
void check(JNIEnv *env) {
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
		throw std::runtime_error("Android speech JNI operation failed");
	}
}
template <class T> class local {
public:
	local(JNIEnv *env, T value) : env_(env), value_(value) {
		check(env);
		if (!value_)
			throw std::runtime_error("Android speech JNI object unavailable");
	}
	~local() { env_->DeleteLocalRef(value_); }
	T get() const noexcept { return value_; }

private:
	JNIEnv *env_;
	T       value_;
};
jmethodID method(JNIEnv *env, jclass type, const char *name, const char *signature) {
	auto result = env->GetMethodID(type, name, signature);
	check(env);
	if (!result)
		throw std::runtime_error("Packaged Android speech bridge API mismatch");
	return result;
}
std::string from_java(JNIEnv *env, jstring value, std::size_t maximum) {
	if (!value)
		throw std::runtime_error("Android speech metadata is null");
	const auto length = env->GetStringLength(value);
	check(env);
	if (length < 0 || std::size_t(length) > maximum)
		throw std::runtime_error("Android speech metadata is oversized");
	const auto *chars = env->GetStringChars(value, nullptr);
	check(env);
	if (!chars)
		throw std::runtime_error("Android speech string unavailable");
	struct release {
		JNIEnv      *env;
		jstring      text;
		const jchar *chars;
		~release() { env->ReleaseStringChars(text, chars); }
	} release{env, value, chars};
	std::string result;
	for (jsize i = 0; i < length; ++i) {
		std::uint32_t point = chars[i];
		if (point >= 0xd800 && point <= 0xdbff) {
			if (++i == length || chars[i] < 0xdc00 || chars[i] > 0xdfff)
				throw std::runtime_error("Invalid Android speech metadata Unicode");
			point = 0x10000 + ((point - 0xd800) << 10) + (chars[i] - 0xdc00);
		} else if (point >= 0xdc00 && point <= 0xdfff)
			throw std::runtime_error("Invalid Android speech metadata Unicode");
		if (point < 0x80)
			result.push_back(static_cast<char>(point));
		else if (point < 0x800) {
			result.push_back(static_cast<char>(0xc0 | (point >> 6)));
			result.push_back(static_cast<char>(0x80 | (point & 63)));
		} else if (point < 0x10000) {
			result.push_back(static_cast<char>(0xe0 | (point >> 12)));
			result.push_back(static_cast<char>(0x80 | ((point >> 6) & 63)));
			result.push_back(static_cast<char>(0x80 | (point & 63)));
		} else {
			result.push_back(static_cast<char>(0xf0 | (point >> 18)));
			result.push_back(static_cast<char>(0x80 | ((point >> 12) & 63)));
			result.push_back(static_cast<char>(0x80 | ((point >> 6) & 63)));
			result.push_back(static_cast<char>(0x80 | (point & 63)));
		}
	}
	if (!core::valid_utf8_text(result, maximum))
		throw std::runtime_error("Invalid Android speech metadata text");
	return result;
}
jstring java_text(JNIEnv *env, std::string_view text) {
	if (text.empty())
		return env->NewString(nullptr, 0);
	auto converted = android_detail::segments(text, 1024u * 1024u);
	if (!converted || converted->size() != 1)
		throw std::runtime_error("Android speech string conversion failed");
	return env->NewString(reinterpret_cast<const jchar *>(converted->front().data()),
	                      static_cast<jsize>(converted->front().size()));
}
struct bridge;
struct callback_registry {
	std::mutex                                     mutex;
	std::uint64_t                                  next = 0;
	std::map<std::uint64_t, std::weak_ptr<bridge>> owners;
};
// Native callbacks can outlive owner teardown; registry storage lasts until process exit.
callback_registry &registry() {
	static auto *value = new callback_registry;
	return *value;
}
std::shared_ptr<bridge> lookup(jlong owner) noexcept {
	try {
		auto           &r = registry();
		std::lock_guard lock(r.mutex);
		auto            at = r.owners.find(static_cast<std::uint64_t>(owner));
		return at == r.owners.end() ? nullptr : at->second.lock();
	} catch (...) {
		return {};
	}
}
struct bridge {
	JavaVM   *vm     = nullptr;
	jobject   object = nullptr;
	jmethodID status = nullptr, failure = nullptr, voices = nullptr, maximum = nullptr,
	          start = nullptr, cancel = nullptr, close = nullptr;
	std::uint64_t          identity = 0;
	std::thread::id        owner    = std::this_thread::get_id();
	android_detail::stream stream;
	~bridge() {
		try {
			auto           &r = registry();
			std::lock_guard lock(r.mutex);
			r.owners.erase(identity);
		} catch (...) {
		}
		if (object)
			try {
				environment scope(vm);
				auto       *env = scope.get();
				env->CallVoidMethod(object, close);
				if (env->ExceptionCheck())
					env->ExceptionClear();
				env->DeleteGlobalRef(object);
			} catch (...) {
			}
	}
	void require_owner() const {
		if (owner != std::this_thread::get_id())
			throw std::runtime_error("Android speech requires its creating owner thread");
	}
	void stop() noexcept {
		stream.cancel();
		if (object)
			try {
				environment scope(vm);
				auto       *env = scope.get();
				env->CallVoidMethod(object, cancel);
				if (env->ExceptionCheck())
					env->ExceptionClear();
			} catch (...) {
			}
	}
};
jboolean JNICALL native_begin([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass type,
                              jlong owner, jlong generation, jint rate, jint format,
                              jint channels) {
	auto state = lookup(owner);
	return state && state->stream.begin(static_cast<std::uint64_t>(generation),
	                                    static_cast<std::uint32_t>(rate),
	                                    static_cast<android_detail::encoding>(format),
	                                    static_cast<std::uint32_t>(channels));
}
jboolean JNICALL native_audio(JNIEnv *env, [[maybe_unused]] jclass type, jlong owner,
                              jlong generation, jbyteArray audio) {
	auto state = lookup(owner);
	if (!state || !audio)
		return false;
	try {
		const auto count = env->GetArrayLength(audio);
		check(env);
		if (count < 0 || count > 64 * 1024 * 1024)
			throw std::runtime_error("Android speech PCM callback exceeds budget");
		std::array<std::byte, 8192> bytes{};
		for (jsize offset = 0; offset < count;) {
			const auto size = std::min<jsize>(static_cast<jsize>(bytes.size()), count - offset);
			env->GetByteArrayRegion(audio, offset, size, reinterpret_cast<jbyte *>(bytes.data()));
			check(env);
			if (!state->stream.audio(static_cast<std::uint64_t>(generation),
			                         std::span(bytes).first(static_cast<std::size_t>(size))))
				return false;
			offset += size;
		}
		return true;
	} catch (...) {
		if (env->ExceptionCheck())
			env->ExceptionClear();
		state->stream.fail(static_cast<std::uint64_t>(generation),
		                   "Android speech PCM callback copy failed", 0);
		return false;
	}
}
jboolean JNICALL native_done([[maybe_unused]] JNIEnv *env, [[maybe_unused]] jclass type,
                             jlong owner, jlong generation, jboolean last) {
	auto state = lookup(owner);
	return state && state->stream.done(static_cast<std::uint64_t>(generation), last);
}
void JNICALL native_failure(JNIEnv *env, [[maybe_unused]] jclass type, jlong owner,
                            jlong generation, jint code, jstring message) {
	auto state = lookup(owner);
	if (!state)
		return;
	try {
		state->stream.fail(static_cast<std::uint64_t>(generation), from_java(env, message, 4096),
		                   code);
	} catch (...) {
		if (env->ExceptionCheck())
			env->ExceptionClear();
		state->stream.fail(static_cast<std::uint64_t>(generation), "Android speech provider failed",
		                   code);
	}
}
class provider final : public speech_provider {
public:
	provider(std::shared_ptr<bridge> owner, speech_profile profile, std::size_t maximum)
	    : state_(std::move(owner)), profile_(std::move(profile)), maximum_(maximum) {}
	~provider() override { cancel(); }
	const speech_profile           &profile() const noexcept override { return profile_; }
	ice::expected<void, diagnostic> start(const speech_request &request,
	                                      speech_output         output) override {
		try {
			state_->require_owner();
			if (!output.active() ||
			    !std::ranges::any_of(
			        profile_.voices,
			        [&](const auto &voice) { return voice.id == request.voice_id; }) ||
			    !std::isfinite(request.rate_multiplier) ||
			    request.rate_multiplier < profile_.minimum_rate ||
			    request.rate_multiplier > profile_.maximum_rate)
				return ice::unexpected(diagnostic{"Invalid Android speech output/voice/rate"});
			auto parts = android_detail::segments(request.text, maximum_);
			if (!parts)
				return ice::unexpected(parts.error());
			environment         scope(state_->vm);
			auto               *env = scope.get();
			local<jclass>       strings(env, env->FindClass("java/lang/String"));
			local<jobjectArray> segments(env, env->NewObjectArray(static_cast<jsize>(parts->size()),
			                                                      strings.get(), nullptr));
			for (std::size_t i = 0; i < parts->size(); ++i) {
				const auto    &part = (*parts)[i];
				local<jstring> text(env,
				                    env->NewString(reinterpret_cast<const jchar *>(part.data()),
				                                   static_cast<jsize>(part.size())));
				env->SetObjectArrayElement(segments.get(), static_cast<jsize>(i), text.get());
				check(env);
			}
			local<jstring> voice(env, java_text(env, request.voice_id));
			auto           generation = state_->stream.start(std::move(output));
			if (!generation)
				return ice::unexpected(generation.error());
			env->CallVoidMethod(state_->object, state_->start, static_cast<jlong>(*generation),
			                    segments.get(), voice.get(), request.rate_multiplier);
			check(env);
			return {};
		} catch (const std::exception &error) {
			state_->stop();
			return ice::unexpected(diagnostic{error.what()});
		} catch (...) {
			state_->stop();
			return ice::unexpected(diagnostic{"Android speech start failed"});
		}
	}
	void cancel() noexcept override { state_->stop(); }

private:
	std::shared_ptr<bridge> state_;
	speech_profile          profile_;
	std::size_t             maximum_;
};
} // namespace
struct android_speech_preparation::state {
	std::shared_ptr<bridge> native;
	std::string             requested_engine;
	bool                    consumed = false;
};
android_speech_preparation::android_speech_preparation(std::unique_ptr<state> value)
    : state_(std::move(value)) {}
android_speech_preparation::~android_speech_preparation() = default;
android_speech_preparation::android_speech_preparation(
    android_speech_preparation &&other) noexcept = default;
android_speech_preparation &
android_speech_preparation::operator=(android_speech_preparation &&other) noexcept = default;
ice::expected<android_speech_preparation, diagnostic>
android_speech_preparation::begin(JavaVM *vm, jobject context, android_speech_options options) {
	try {
		if (!context || !core::valid_utf8_text(options.requested_engine, 512))
			return ice::unexpected(diagnostic{"Invalid Android speech context/engine request"});
		environment   scope(vm);
		auto         *env = scope.get();
		local<jclass> context_type(env, env->GetObjectClass(context));
		auto          loader_method =
		    method(env, context_type.get(), "getClassLoader", "()Ljava/lang/ClassLoader;");
		local<jobject> loader(env, env->CallObjectMethod(context, loader_method));
		local<jclass>  loader_type(env, env->GetObjectClass(loader.get()));
		auto           load =
		    method(env, loader_type.get(), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
		local<jstring> class_name(env, java_text(env, "org.ice.audio.AndroidSpeechBridge"));
		local<jclass>  type(
            env, static_cast<jclass>(env->CallObjectMethod(loader.get(), load, class_name.get())));
		JNINativeMethod methods[] = {
		    {const_cast<char *>("nativeBegin"), const_cast<char *>("(JJIII)Z"),
		     reinterpret_cast<void *>(native_begin)},
		    {const_cast<char *>("nativeAudio"), const_cast<char *>("(JJ[B)Z"),
		     reinterpret_cast<void *>(native_audio)},
		    {const_cast<char *>("nativeDone"), const_cast<char *>("(JJZ)Z"),
		     reinterpret_cast<void *>(native_done)},
		    {const_cast<char *>("nativeFailure"), const_cast<char *>("(JJILjava/lang/String;)V"),
		     reinterpret_cast<void *>(native_failure)}};
		if (env->RegisterNatives(type.get(), methods, 4) != JNI_OK) {
			check(env);
			throw std::runtime_error("Android speech native callback registration failed");
		}
		auto native     = std::make_shared<bridge>();
		native->vm      = vm;
		native->status  = method(env, type.get(), "status", "()I");
		native->failure = method(env, type.get(), "failure", "()Ljava/lang/String;");
		native->voices  = method(env, type.get(), "voices", "()[Ljava/lang/String;");
		native->maximum = method(env, type.get(), "maximumCharacters", "()I");
		native->start =
		    method(env, type.get(), "start", "(J[Ljava/lang/String;Ljava/lang/String;F)V");
		native->cancel = method(env, type.get(), "cancel", "()V");
		native->close  = method(env, type.get(), "close", "()V");
		{
			auto           &r = registry();
			std::lock_guard lock(r.mutex);
			if (r.next == std::uint64_t(std::numeric_limits<jlong>::max()))
				throw std::runtime_error("Android speech callback identities exhausted");
			native->identity = ++r.next;
			r.owners.emplace(native->identity, native);
		}
		auto constructor =
		    method(env, type.get(), "<init>", "(Landroid/content/Context;Ljava/lang/String;J)V");
		local<jstring> requested(env, java_text(env, options.requested_engine));
		local<jobject> object(env, env->NewObject(type.get(), constructor, context, requested.get(),
		                                          static_cast<jlong>(native->identity)));
		native->object = env->NewGlobalRef(object.get());
		if (env->ExceptionCheck() || !native->object) {
			if (env->ExceptionCheck())
				env->ExceptionClear();
			env->CallVoidMethod(object.get(), native->close);
			if (env->ExceptionCheck())
				env->ExceptionClear();
			throw std::runtime_error("Android speech bridge ownership allocation failed");
		}
		auto result              = std::make_unique<state>();
		result->native           = std::move(native);
		result->requested_engine = std::move(options.requested_engine);
		return android_speech_preparation(std::move(result));
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	} catch (...) {
		return ice::unexpected(diagnostic{"Android speech preparation failed"});
	}
}
ice::expected<std::optional<android_speech_ready>, diagnostic> android_speech_preparation::poll() {
	try {
		if (!state_ || state_->consumed)
			return ice::unexpected(diagnostic{"Empty/consumed Android speech preparation"});
		auto native = state_->native;
		native->require_owner();
		environment scope(native->vm);
		auto       *env    = scope.get();
		const auto  status = env->CallIntMethod(native->object, native->status);
		check(env);
		if (status == 0)
			return std::optional<android_speech_ready>{};
		if (status != 1) {
			local<jstring> message(
			    env, static_cast<jstring>(env->CallObjectMethod(native->object, native->failure)));
			return ice::unexpected(diagnostic{from_java(env, message.get(), 4096)});
		}
		const auto maximum = env->CallIntMethod(native->object, native->maximum);
		check(env);
		if (maximum < 2 || maximum > 1024 * 1024)
			return ice::unexpected(diagnostic{"Unsupported Android speech native character limit"});
		local<jobjectArray> voices(
		    env, static_cast<jobjectArray>(env->CallObjectMethod(native->object, native->voices)));
		const auto count = env->GetArrayLength(voices.get());
		check(env);
		if (count <= 0 || count > 65536 * 3 || count % 3)
			return ice::unexpected(diagnostic{"Invalid Android speech voice inventory"});
		speech_profile profile;
		profile.provider = "android_installed_tts";
		profile.voices.reserve(static_cast<std::size_t>(count / 3));
		for (jsize i = 0; i < count; i += 3) {
			local<jstring> id(env,
			                  static_cast<jstring>(env->GetObjectArrayElement(voices.get(), i)));
			local<jstring> name(
			    env, static_cast<jstring>(env->GetObjectArrayElement(voices.get(), i + 1)));
			local<jstring> locale(
			    env, static_cast<jstring>(env->GetObjectArrayElement(voices.get(), i + 2)));
			profile.voices.push_back({from_java(env, id.get(), 512),
			                          from_java(env, name.get(), 512),
			                          from_java(env, locale.get(), 128)});
		}
		android_speech_ready ready{std::make_unique<provider>(native, std::move(profile),
		                                                      static_cast<std::size_t>(maximum)),
		                           state_->requested_engine};
		state_->consumed = true;
		state_->native.reset();
		return std::optional<android_speech_ready>(std::move(ready));
	} catch (const std::exception &error) {
		return ice::unexpected(diagnostic{error.what()});
	} catch (...) {
		return ice::unexpected(diagnostic{"Android speech preparation poll failed"});
	}
}
} // namespace ice::audio
