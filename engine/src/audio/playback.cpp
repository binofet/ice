#include "ice/audio/playback.h"
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <deque>
#include <limits>
#include <new>
#include <string_view>
#include <thread>
#include <utility>

namespace ice::audio {
namespace {
struct audio_failure {
	diagnostic detail;
};
[[noreturn]] void fail(std::string message, std::int32_t code = 0) {
	throw audio_failure{{std::move(message), code}};
}
void check_al(const char *operation) {
	const auto error = alGetError();
	if (error != AL_NO_ERROR)
		fail(operation, error);
}
template <typename Function> auto guarded(Function function) -> decltype(function()) {
	try {
		return function();
	} catch (const audio_failure &error) {
		return ice::unexpected(error.detail);
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Audio allocation failed"});
	} catch (const std::exception &) {
		return ice::unexpected(diagnostic{"Audio operation failed"});
	}
}
struct context_scope {
	ALCcontext *previous = nullptr;
	explicit context_scope(ALCcontext *context) : previous(alcGetThreadContext()) {
		if (!alcSetThreadContext(context))
			fail("Cannot enter playback thread context");
	}
	~context_scope() { alcSetThreadContext(previous); }
	context_scope(const context_scope &)            = delete;
	context_scope &operator=(const context_scope &) = delete;
};
bool finite(math::vec3 value) {
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
bool position_valid(math::vec3 value) {
	return finite(value) && std::abs(value.x) <= 1e6f && std::abs(value.y) <= 1e6f &&
	       std::abs(value.z) <= 1e6f;
}
void validate(const voice_parameters &value, std::uint32_t channels, bool effects) {
	if ((value.spatial && channels != 1) || !position_valid(value.position) ||
	    !position_valid(value.velocity) || !std::isfinite(value.gain) || value.gain < 0 ||
	    value.gain > 16 || !std::isfinite(value.pitch) || value.pitch <= 0 || value.pitch > 16 ||
	    !std::isfinite(value.reference_distance) || value.reference_distance <= 0 ||
	    value.reference_distance > 1e6f || !std::isfinite(value.rolloff) || value.rolloff < 0 ||
	    value.rolloff > 1000 || !std::isfinite(value.high_frequency_gain) ||
	    value.high_frequency_gain < 0 || value.high_frequency_gain > 1 ||
	    !std::isfinite(value.reverb_send) || value.reverb_send < 0 || value.reverb_send > 1)
		fail("Invalid playback source parameters (spatial sources require mono)");
	if (!effects && (value.high_frequency_gain != 1 || value.reverb_send != 0))
		fail("Requested playback effects are unavailable");
}
std::string                text(const char *value) { return value ? value : "unavailable"; }
std::atomic<std::uint64_t> next_owner{1};
std::uint64_t              reserve_owner() {
    auto candidate = next_owner.load();
    for (;;) {
        if (candidate == std::numeric_limits<std::uint64_t>::max())
            fail("Playback device identity exhausted");
        if (next_owner.compare_exchange_weak(candidate, candidate + 1))
            return candidate;
    }
}
} // namespace

struct playback_device::state {
	struct block {
		ALuint        native = 0;
		std::size_t   bytes  = 0;
		std::uint64_t frames = 0;
	};
	struct voice {
		ALuint            source = 0, direct_filter = 0, send_filter = 0;
		std::uint64_t     generation = 1;
		voice_description description;
		std::deque<block> blocks;
		voice_status      status;
		bool              playing = false, paused = false;
	};
	playback_options      options;
	playback_capabilities profile;
	ALCdevice            *device  = nullptr;
	ALCcontext           *context = nullptr;
	ALuint                effect = 0, effect_slot = 0;
	std::uint64_t         owner        = 0;
	std::thread::id       owner_thread = std::this_thread::get_id();
	std::vector<voice>    voices;
	std::size_t           queued_bytes = 0, queued_blocks = 0;
	bool                  suspended = false, disconnected = false;
	~state() {
		if (context) {
			auto *previous = alcGetThreadContext();
			if (alcSetThreadContext(context)) {
				for (auto &item : voices) {
					if (item.source) {
						alSourceStop(item.source);
						alDeleteSources(1, &item.source);
					}
					for (auto &queued : item.blocks)
						alDeleteBuffers(1, &queued.native);
					if (item.direct_filter)
						alDeleteFilters(1, &item.direct_filter);
					if (item.send_filter)
						alDeleteFilters(1, &item.send_filter);
				}
				if (effect_slot)
					alDeleteAuxiliaryEffectSlots(1, &effect_slot);
				if (effect)
					alDeleteEffects(1, &effect);
				alcSetThreadContext(previous == context ? nullptr : previous);
			}
			alcDestroyContext(context);
		}
		if (device)
			alcCloseDevice(device);
	}
	context_scope enter() {
		if (owner_thread != std::this_thread::get_id())
			fail("Playback requires its creating owner thread");
		if (disconnected)
			fail("Playback output disconnected; recreate explicitly");
		if (!profile.loopback && alcIsExtensionPresent(device, "ALC_EXT_disconnect")) {
			ALCint connected = ALC_TRUE;
			alcGetIntegerv(device, ALC_CONNECTED, 1, &connected);
			if (connected != ALC_TRUE) {
				disconnected = true;
				fail("Playback output disconnected; recreate explicitly");
			}
		}
		return context_scope(context);
	}
	voice &get(voice_handle handle) {
		if (handle.owner != owner || handle.slot >= voices.size() || !handle.generation ||
		    !voices[handle.slot].source || voices[handle.slot].generation != handle.generation)
			fail("Foreign or retired playback voice handle");
		return voices[handle.slot];
	}
	void controls(voice &item, const voice_parameters &value) {
		alSourcei(item.source, AL_SOURCE_RELATIVE, value.spatial ? AL_FALSE : AL_TRUE);
		alSourcei(item.source, AL_SOURCE_SPATIALIZE_SOFT, value.spatial ? AL_TRUE : AL_FALSE);
		alSource3f(item.source, AL_POSITION, value.position.x, value.position.y, value.position.z);
		alSource3f(item.source, AL_VELOCITY, value.velocity.x, value.velocity.y, value.velocity.z);
		alSourcef(item.source, AL_GAIN, value.gain);
		alSourcef(item.source, AL_PITCH, value.pitch);
		alSourcef(item.source, AL_REFERENCE_DISTANCE, value.reference_distance);
		alSourcef(item.source, AL_ROLLOFF_FACTOR, value.spatial ? value.rolloff : 0.f);
		if (profile.effects) {
			alFilterf(item.direct_filter, AL_LOWPASS_GAINHF, value.high_frequency_gain);
			alSourcei(item.source, AL_DIRECT_FILTER, static_cast<ALint>(item.direct_filter));
			alFilterf(item.send_filter, AL_LOWPASS_GAIN, value.reverb_send);
			alSource3i(item.source, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(effect_slot), 0,
			           static_cast<ALint>(item.send_filter));
		}
		check_al("Cannot configure playback source");
		item.description.parameters = value;
	}
	void release_blocks(voice &item) {
		alSourceStop(item.source);
		alSourcei(item.source, AL_BUFFER, 0);
		check_al("Cannot detach playback queue");
		for (const auto &queued : item.blocks) {
			alDeleteBuffers(1, &queued.native);
			queued_bytes -= queued.bytes;
			--queued_blocks;
		}
		item.blocks.clear();
		item.status  = {};
		item.playing = item.paused = false;
		check_al("Cannot retire playback queue");
	}
	void refresh(voice &item) {
		ALint processed = 0;
		alGetSourcei(item.source, AL_BUFFERS_PROCESSED, &processed);
		check_al("Cannot inspect processed playback queue");
		if (processed < 0 || std::size_t(processed) > item.blocks.size())
			fail("Native playback queue accounting mismatch");
		while (processed-- > 0) {
			ALuint native = 0;
			alSourceUnqueueBuffers(item.source, 1, &native);
			check_al("Cannot reclaim playback block");
			if (native != item.blocks.front().native)
				fail("Native playback queue order mismatch");
			queued_bytes -= item.blocks.front().bytes;
			--queued_blocks;
			item.status.queued_frames -= item.blocks.front().frames;
			item.blocks.pop_front();
			alDeleteBuffers(1, &native);
			check_al("Cannot delete consumed playback block");
		}
		item.status.queued_blocks = item.blocks.size();
		if (item.paused || (suspended && item.playing)) {
			item.status.phase = voice_phase::paused;
			return;
		}
		if (!item.playing) {
			item.status.phase = voice_phase::stopped;
			return;
		}
		if (item.blocks.empty()) {
			const auto phase =
			    item.status.end_of_stream ? voice_phase::completed : voice_phase::starved;
			if (phase == voice_phase::starved && item.status.phase != phase &&
			    item.status.starvation_count != std::numeric_limits<std::uint64_t>::max())
				++item.status.starvation_count;
			item.status.phase = phase;
			return;
		}
		ALint native = 0;
		alGetSourcei(item.source, AL_SOURCE_STATE, &native);
		if (native != AL_PLAYING)
			alSourcePlay(item.source);
		check_al("Cannot service requested playback stream");
		item.status.phase = voice_phase::playing;
	}
};

playback_device::playback_device(std::unique_ptr<state> value) : state_(std::move(value)) {}
playback_device::~playback_device()                                           = default;
playback_device::playback_device(playback_device &&other) noexcept            = default;
playback_device &playback_device::operator=(playback_device &&other) noexcept = default;

ice::expected<playback_device, diagnostic> playback_device::create(playback_options options) {
	return guarded([&]() -> ice::expected<playback_device, diagnostic> {
		if ((options.mode != playback_mode::native && options.mode != playback_mode::loopback) ||
		    (options.hrtf != hrtf_policy::disabled && options.hrtf != hrtf_policy::prefer &&
		     options.hrtf != hrtf_policy::require) ||
		    options.sample_rate < 8000 || options.sample_rate > 192000 ||
		    !options.limits.max_voices || options.limits.max_voices > 65536 ||
		    options.limits.max_queued_bytes < sizeof(float) || !options.limits.max_queued_blocks)
			fail("Invalid playback mode, rate or resource profile");
		auto result              = std::make_unique<state>();
		result->options          = options;
		result->owner            = reserve_owner();
		result->profile.loopback = options.mode == playback_mode::loopback;
		result->device =
		    result->profile.loopback ? alcLoopbackOpenDeviceSOFT(nullptr) : alcOpenDevice(nullptr);
		if (!result->device)
			fail("Cannot open requested OpenAL Soft output");
		if (!result->profile.loopback &&
		    !alcIsExtensionPresent(result->device, "ALC_ENUMERATE_ALL_EXT"))
			fail("Native playback requires actual output identity discovery");
		// ALC_DEVICE_SPECIFIER is the library's generic name in this pinned release.
		// The opened device's ALL_DEVICES string identifies its actual native backend.
		const auto *output = alcGetString(result->device, ALC_ALL_DEVICES_SPECIFIER);
		if (!output || alcGetError(result->device) != ALC_NO_ERROR)
			fail("Cannot identify the opened playback output");
		result->profile.output                   = output;
		std::string_view           actual_output = result->profile.output;
		constexpr std::string_view prefix        = "OpenAL Soft on ";
		if (actual_output.starts_with(prefix))
			actual_output.remove_prefix(prefix.size());
		if (!result->profile.loopback &&
		    (actual_output == "No Output" || actual_output == "Wave File Writer"))
			fail("Requested physical output resolved to a nonphysical backend: " +
			     result->profile.output);
		if (!alcIsExtensionPresent(result->device, "ALC_EXT_thread_local_context"))
			fail("Playback requires thread-local native contexts");
		result->profile.hrtf = alcIsExtensionPresent(result->device, "ALC_SOFT_HRTF");
		if (options.hrtf == hrtf_policy::require && !result->profile.hrtf)
			fail("Required HRTF support is unavailable");
		std::vector<ALCint> attributes{ALC_FREQUENCY, static_cast<ALCint>(options.sample_rate)};
		if (result->profile.loopback)
			attributes.insert(attributes.end(), {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,
			                                     ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT});
		if (result->profile.hrtf)
			attributes.insert(
			    attributes.end(),
			    {ALC_HRTF_SOFT, options.hrtf == hrtf_policy::disabled ? ALC_FALSE : ALC_TRUE});
		attributes.push_back(0);
		result->context = alcCreateContext(result->device, attributes.data());
		if (!result->context)
			fail("Cannot prepare requested playback context", alcGetError(result->device));
		auto scope = result->enter();
		result->profile.renderer =
		    text(alGetString(AL_RENDERER)) + " / " + text(alGetString(AL_VERSION));
		result->profile.float_pcm = alIsExtensionPresent("AL_EXT_FLOAT32");
		result->profile.effects   = alcIsExtensionPresent(result->device, "ALC_EXT_EFX");
		if (!result->profile.float_pcm || !alIsExtensionPresent("AL_SOFT_source_spatialize"))
			fail("Playback requires float PCM and explicit spatialization control");
		ALCint value = 0;
		alcGetIntegerv(result->device, ALC_FREQUENCY, 1, &value);
		result->profile.sample_rate = static_cast<std::uint32_t>(value);
		if (result->profile.hrtf) {
			alcGetIntegerv(result->device, ALC_HRTF_SOFT, 1, &value);
			result->profile.hrtf_active = value == ALC_TRUE;
			if (result->profile.hrtf_active)
				result->profile.hrtf_name =
				    text(alcGetString(result->device, ALC_HRTF_SPECIFIER_SOFT));
		}
		if (options.hrtf == hrtf_policy::require && !result->profile.hrtf_active)
			fail("Required HRTF activation was rejected");
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		if (result->profile.effects) {
			alGenEffects(1, &result->effect);
			alEffecti(result->effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
			alGenAuxiliaryEffectSlots(1, &result->effect_slot);
			alAuxiliaryEffectSloti(result->effect_slot, AL_EFFECTSLOT_EFFECT,
			                       static_cast<ALint>(result->effect));
		}
		check_al("Cannot initialize playback mixer/effects");
		result->voices.reserve(options.limits.max_voices);
		return playback_device(std::move(result));
	});
}
playback_capabilities playback_device::capabilities() const {
	return state_ ? state_->profile : playback_capabilities{};
}
ice::expected<void, diagnostic> playback_device::set_listener(const listener_pose &pose) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		if (!position_valid(pose.position) || !position_valid(pose.velocity) ||
		    !finite(pose.forward) || !finite(pose.up))
			fail("Invalid playback listener pose");
		// Normalize in double to handle every finite float direction without a
		// spurious overflow/underflow. Cross products avoid cancellation when
		// rejecting an exactly parallel up vector.
		const auto forward_length = glm::length(glm::dvec3(pose.forward));
		const auto up_length      = glm::length(glm::dvec3(pose.up));
		if (forward_length == 0 || up_length == 0)
			fail("Invalid playback listener direction/up");
		const auto forward      = glm::dvec3(pose.forward) / forward_length;
		const auto right        = glm::cross(forward, glm::dvec3(pose.up) / up_length);
		const auto right_length = glm::length(right);
		if (right_length == 0)
			fail("Invalid playback listener up vector");
		const auto                 up = glm::cross(right / right_length, forward);
		const std::array<float, 6> orientation{float(forward.x), float(forward.y), float(forward.z),
		                                       float(up.x),      float(up.y),      float(up.z)};
		auto                       scope = state_->enter();
		alListener3f(AL_POSITION, pose.position.x, pose.position.y, pose.position.z);
		alListener3f(AL_VELOCITY, pose.velocity.x, pose.velocity.y, pose.velocity.z);
		alListenerfv(AL_ORIENTATION, orientation.data());
		check_al("Cannot update playback listener");
		return {};
	});
}
ice::expected<voice_handle, diagnostic>
playback_device::create_voice(const voice_description &description) {
	return guarded([&]() -> ice::expected<voice_handle, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		if ((description.channels != 1 && description.channels != 2) ||
		    description.sample_rate < 8000 || description.sample_rate > 192000)
			fail("Invalid playback stream format");
		validate(description.parameters, description.channels, state_->profile.effects);
		auto        scope = state_->enter();
		std::size_t slot  = 0;
		while (slot < state_->voices.size() &&
		       (state_->voices[slot].source ||
		        state_->voices[slot].generation == std::numeric_limits<std::uint64_t>::max()))
			++slot;
		if (slot == state_->options.limits.max_voices)
			fail("Playback voice budget exhausted");
		if (slot == state_->voices.size())
			state_->voices.emplace_back();
		auto &item       = state_->voices[slot];
		item.description = description;
		try {
			alGenSources(1, &item.source);
			if (state_->profile.effects) {
				alGenFilters(1, &item.direct_filter);
				alFilteri(item.direct_filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
				alGenFilters(1, &item.send_filter);
				alFilteri(item.send_filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
			}
			check_al("Cannot allocate playback source");
			state_->controls(item, description.parameters);
		} catch (...) {
			if (item.source)
				alDeleteSources(1, &item.source);
			if (item.direct_filter)
				alDeleteFilters(1, &item.direct_filter);
			if (item.send_filter)
				alDeleteFilters(1, &item.send_filter);
			item.source = item.direct_filter = item.send_filter = 0;
			throw;
		}
		return voice_handle{state_->owner, static_cast<std::uint32_t>(slot), item.generation};
	});
}
ice::expected<void, diagnostic> playback_device::configure(voice_handle            handle,
                                                           const voice_parameters &parameters) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto &item = state_->get(handle);
		validate(parameters, item.description.channels, state_->profile.effects);
		auto scope = state_->enter();
		state_->controls(item, parameters);
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::queue(voice_handle           handle,
                                                       std::span<const float> samples) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto &item = state_->get(handle);
		if (item.status.end_of_stream || samples.empty() ||
		    samples.size() % item.description.channels ||
		    samples.size() > std::size_t(std::numeric_limits<ALsizei>::max()) / sizeof(float))
			fail("Invalid/closed playback queue block");
		const auto bytes = samples.size_bytes();
		if (bytes > state_->options.limits.max_queued_bytes - state_->queued_bytes ||
		    state_->queued_blocks >= state_->options.limits.max_queued_blocks)
			fail("Playback queue budget exhausted");
		if (std::any_of(samples.begin(), samples.end(), [](float value) {
			    return !std::isfinite(value) || value < -1 || value > 1;
		    }))
			fail("Playback PCM must be finite and normalized");
		auto scope = state_->enter();
		state_->refresh(item);
		if (item.blocks.empty()) {
			// A stopped OpenAL source treats newly queued buffers as processed until
			// rewound/restarted. Reset only an empty queue, preserving all live PCM.
			alSourceRewind(item.source);
			check_al("Cannot reset empty playback queue");
		}
		item.blocks.push_back({0, bytes, samples.size() / item.description.channels});
		auto &queued = item.blocks.back();
		try {
			alGenBuffers(1, &queued.native);
			alBufferData(queued.native,
			             item.description.channels == 1 ? AL_FORMAT_MONO_FLOAT32
			                                            : AL_FORMAT_STEREO_FLOAT32,
			             samples.data(), static_cast<ALsizei>(bytes),
			             static_cast<ALsizei>(item.description.sample_rate));
			check_al("Cannot upload playback PCM block");
			alSourceQueueBuffers(item.source, 1, &queued.native);
			check_al("Cannot publish playback PCM block");
		} catch (...) {
			if (queued.native)
				alDeleteBuffers(1, &queued.native);
			item.blocks.pop_back();
			throw;
		}
		state_->queued_bytes += bytes;
		++state_->queued_blocks;
		item.status.queued_frames += queued.frames;
		item.status.queued_blocks = item.blocks.size();
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::finish(voice_handle handle) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto scope                               = state_->enter();
		state_->get(handle).status.end_of_stream = true;
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::play(voice_handle handle) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto  scope  = state_->enter();
		auto &item   = state_->get(handle);
		item.playing = true;
		item.paused  = false;
		state_->refresh(item);
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::pause(voice_handle handle) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto  scope = state_->enter();
		auto &item  = state_->get(handle);
		alSourcePause(item.source);
		check_al("Cannot pause playback voice");
		item.paused = true;
		state_->refresh(item);
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::stop(voice_handle handle) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto scope = state_->enter();
		state_->release_blocks(state_->get(handle));
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::retire(voice_handle handle) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto  scope = state_->enter();
		auto &item  = state_->get(handle);
		state_->release_blocks(item);
		alDeleteSources(1, &item.source);
		if (item.direct_filter)
			alDeleteFilters(1, &item.direct_filter);
		if (item.send_filter)
			alDeleteFilters(1, &item.send_filter);
		check_al("Cannot retire playback source");
		item.source = item.direct_filter = item.send_filter = 0;
		if (item.generation != std::numeric_limits<std::uint64_t>::max())
			++item.generation;
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::poll() {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto scope = state_->enter();
		for (auto &item : state_->voices)
			if (item.source)
				state_->refresh(item);
		return {};
	});
}
ice::expected<voice_status, diagnostic> playback_device::inspect(voice_handle handle) {
	return guarded([&]() -> ice::expected<voice_status, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto  scope = state_->enter();
		auto &item  = state_->get(handle);
		state_->refresh(item);
		return item.status;
	});
}
ice::expected<void, diagnostic> playback_device::suspend() {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto scope = state_->enter();
		if (state_->suspended)
			return {};
		for (auto &item : state_->voices)
			if (item.source && item.playing)
				alSourcePause(item.source);
		check_al("Cannot suspend playback voices");
		state_->suspended = true;
		if (!state_->profile.loopback &&
		    alcIsExtensionPresent(state_->device, "ALC_SOFT_pause_device")) {
			alcDevicePauseSOFT(state_->device);
			const auto error = alcGetError(state_->device);
			if (error != ALC_NO_ERROR)
				fail("Cannot suspend playback output", error);
		}
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::resume() {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		auto scope = state_->enter();
		if (!state_->suspended)
			return {};
		if (!state_->profile.loopback &&
		    alcIsExtensionPresent(state_->device, "ALC_SOFT_pause_device")) {
			alcDeviceResumeSOFT(state_->device);
			const auto error = alcGetError(state_->device);
			if (error != ALC_NO_ERROR)
				fail("Cannot resume playback output", error);
		}
		state_->suspended = false;
		for (auto &item : state_->voices)
			if (item.source)
				state_->refresh(item);
		return {};
	});
}
ice::expected<void, diagnostic> playback_device::render(std::span<float> samples) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			fail("Empty playback owner");
		if (!state_->profile.loopback || samples.size() % 2 ||
		    samples.size() / 2 > std::size_t(std::numeric_limits<ALCsizei>::max()))
			fail("Offline rendering requires an even stereo span and a loopback device");
		auto scope = state_->enter();
		if (state_->suspended) {
			std::fill(samples.begin(), samples.end(), 0.f);
			return {};
		}
		for (auto &item : state_->voices)
			if (item.source)
				state_->refresh(item);
		if (!samples.empty())
			alcRenderSamplesSOFT(state_->device, samples.data(),
			                     static_cast<ALCsizei>(samples.size() / 2));
		const auto error = alcGetError(state_->device);
		if (error != ALC_NO_ERROR)
			fail("Offline playback render failed", error);
		for (auto &item : state_->voices)
			if (item.source)
				state_->refresh(item);
		return {};
	});
}
} // namespace ice::audio
