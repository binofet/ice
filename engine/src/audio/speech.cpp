#include "ice/audio/speech.h"
#include "ice/core/utf8.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>

namespace ice::audio {
namespace {
struct speech_failure {
	diagnostic error;
};
[[noreturn]] void reject(std::string message) { throw speech_failure{{std::move(message)}}; }
template <typename Function> auto guarded(Function function) -> decltype(function()) {
	try {
		return function();
	} catch (const speech_failure &error) {
		return ice::unexpected(error.error);
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Speech allocation failed"});
	} catch (...) {
		return ice::unexpected(diagnostic{"Speech operation failed"});
	}
}
std::atomic<std::uint64_t> next_speech_owner{1};
std::uint64_t              reserve_owner() {
    auto value = next_speech_owner.load();
    for (;;) {
        if (value == std::numeric_limits<std::uint64_t>::max())
            reject("Speech owner identity exhausted");
        if (next_speech_owner.compare_exchange_weak(value, value + 1))
            return value;
    }
}
} // namespace
struct speech_output::state {
	mutable std::mutex    mutex;
	speech_request        request;
	speech_limits         limits;
	speech_phase          phase = speech_phase::running;
	std::deque<pcm_data>  chunks;
	std::uint32_t         sample_rate = 0, channels = 0;
	std::uint64_t         generated_frames = 0, delivered_frames = 0, buffered_frames = 0;
	std::size_t           buffered_bytes = 0;
	std::array<char, 512> error{};
	std::size_t           error_size  = 0;
	std::int32_t          native_code = 0;
	void                  discard() noexcept {
        chunks.clear();
        buffered_bytes  = 0;
        buffered_frames = 0;
	}
	void fault(std::string_view message, std::int32_t code = 0) noexcept {
		if (phase != speech_phase::running)
			return;
		phase = speech_phase::failed;
		discard();
		native_code = code;
		if (message.empty())
			message = "Speech provider failed";
		auto length = std::min(message.size(), error.size() - 1);
		bool valid  = false;
		// Inspect at most the retained prefix and its three possible partial UTF-8
		// suffix bytes, never an unbounded provider diagnostic.
		for (std::size_t trim = 0; trim < 4 && trim < length; ++trim) {
			if (core::valid_utf8_text(message.substr(0, length - trim))) {
				length -= trim;
				valid = true;
				break;
			}
		}
		if (!valid) {
			message = "Speech provider failed (invalid diagnostic text)";
			length  = message.size();
		}
		std::memcpy(error.data(), message.data(), length);
		error_size = length;
	}
	speech_status snapshot() const {
		speech_status result;
		result.phase            = phase;
		result.request_id       = request.id;
		result.voice_id         = request.voice_id;
		result.sample_rate      = sample_rate;
		result.channels         = channels;
		result.generated_frames = generated_frames;
		result.delivered_frames = delivered_frames;
		result.buffered_frames  = buffered_frames;
		result.buffered_chunks  = chunks.size();
		result.error            = {std::string(error.data(), error_size), native_code};
		return result;
	}
};
speech_output::speech_output(std::weak_ptr<state> value) : state_(std::move(value)) {}
bool speech_output::append(std::uint32_t sample_rate, std::uint32_t channels,
                           std::span<const float> samples) const noexcept {
	try {
		auto state = state_.lock();
		if (!state)
			return false;
		std::lock_guard lock(state->mutex);
		if (state->phase != speech_phase::running)
			return false;
		if (sample_rate < 8000 || sample_rate > 192000 || (channels != 1 && channels != 2) ||
		    samples.empty() || samples.size() % channels ||
		    (state->sample_rate &&
		     (state->sample_rate != sample_rate || state->channels != channels))) {
			state->fault("Invalid/changing speech PCM format");
			return false;
		}
		if (samples.size() >
		        (state->limits.max_buffered_bytes - state->buffered_bytes) / sizeof(float) ||
		    state->chunks.size() >= state->limits.max_buffered_chunks) {
			state->fault("Speech pending PCM budget exhausted");
			return false;
		}
		const auto frames = static_cast<std::uint64_t>(samples.size() / channels);
		if (frames > std::uint64_t(state->limits.max_duration_seconds) * sample_rate -
		                 state->generated_frames) {
			state->fault("Speech generated duration budget exhausted");
			return false;
		}
		if (std::any_of(samples.begin(), samples.end(), [](float sample) {
			    return !std::isfinite(sample) || sample < -1 || sample > 1;
		    })) {
			state->fault("Speech PCM must be finite normalized float samples");
			return false;
		}
		try {
			pcm_data chunk;
			chunk.sample_rate = sample_rate;
			chunk.channels    = channels;
			if (samples.size() > chunk.samples.max_size()) {
				state->fault("Speech PCM exceeds native sample container size");
				return false;
			}
			chunk.samples.assign(samples.begin(), samples.end());
			state->chunks.push_back(std::move(chunk));
		} catch (...) {
			state->fault("Speech PCM allocation failed");
			return false;
		}
		state->sample_rate = sample_rate;
		state->channels    = channels;
		state->generated_frames += frames;
		state->buffered_frames += frames;
		state->buffered_bytes += samples.size_bytes();
		return true;
	} catch (...) {
		return false;
	}
}
void speech_output::complete() const noexcept {
	try {
		if (auto state = state_.lock()) {
			std::lock_guard lock(state->mutex);
			if (state->phase == speech_phase::running)
				state->phase = speech_phase::completed;
		}
	} catch (...) {
	}
}
void speech_output::fail(std::string_view message, std::int32_t native_code) const noexcept {
	try {
		if (auto state = state_.lock()) {
			std::lock_guard lock(state->mutex);
			state->fault(message, native_code);
		}
	} catch (...) {
	}
}
bool speech_output::active() const noexcept {
	try {
		if (auto state = state_.lock()) {
			std::lock_guard lock(state->mutex);
			return state->phase == speech_phase::running;
		}
	} catch (...) {
	}
	return false;
}
struct speech_jobs::state {
	std::unique_ptr<speech_provider>      provider;
	speech_profile                        profile;
	speech_limits                         limits;
	std::thread::id                       thread = std::this_thread::get_id();
	std::uint64_t                         owner = 0, generation = 0;
	std::shared_ptr<speech_output::state> current;
	bool                                  native_cancelled = true;
	void                                  owner_thread() const {
        if (thread != std::this_thread::get_id())
            reject("Speech jobs require their creating owner thread");
	}
	speech_output::state &get(speech_handle handle) const {
		owner_thread();
		if (!current || !handle.generation || handle.owner != owner ||
		    handle.generation != generation)
			reject("Foreign or stale speech handle");
		return *current;
	}
	void stop_native() noexcept {
		if (!native_cancelled) {
			native_cancelled = true;
			provider->cancel();
		}
	}
	~state() {
		if (current) {
			try {
				std::lock_guard lock(current->mutex);
				if (current->phase == speech_phase::running ||
				    (current->phase == speech_phase::completed && !current->chunks.empty()))
					current->phase = speech_phase::cancelled;
				current->discard();
			} catch (...) {
			}
		}
		if (provider)
			stop_native();
	}
};
speech_jobs::speech_jobs(std::unique_ptr<state> value) : state_(std::move(value)) {}
speech_jobs::~speech_jobs()                                       = default;
speech_jobs::speech_jobs(speech_jobs &&other) noexcept            = default;
speech_jobs &speech_jobs::operator=(speech_jobs &&other) noexcept = default;
ice::expected<speech_jobs, diagnostic>
speech_jobs::create(std::unique_ptr<speech_provider> provider, speech_limits limits) {
	return guarded([&]() -> ice::expected<speech_jobs, diagnostic> {
		if (!provider || !limits.max_text_bytes || limits.max_text_bytes > 16u * 1024u * 1024u ||
		    limits.max_buffered_bytes < sizeof(float) ||
		    limits.max_buffered_bytes > 1024u * 1024u * 1024u || !limits.max_buffered_chunks ||
		    limits.max_buffered_chunks > 1000000 || !limits.max_duration_seconds ||
		    limits.max_duration_seconds > 86400)
			reject("Invalid speech provider/resource profile");
		auto result      = std::make_unique<state>();
		result->provider = std::move(provider);
		result->profile  = result->provider->profile();
		result->limits   = limits;
		if (result->profile.provider.empty() ||
		    !core::valid_utf8_text(result->profile.provider, 256) ||
		    result->profile.voices.size() > 65536 || !std::isfinite(result->profile.minimum_rate) ||
		    !std::isfinite(result->profile.maximum_rate) || result->profile.minimum_rate <= 0 ||
		    result->profile.minimum_rate > 1 || result->profile.maximum_rate < 1)
			reject("Invalid speech discovery profile");
		auto &voices = result->profile.voices;
		std::sort(voices.begin(), voices.end(),
		          [](const auto &left, const auto &right) { return left.id < right.id; });
		for (std::size_t index = 0; index < voices.size(); ++index) {
			const auto &voice = voices[index];
			if (voice.id.empty() || !core::valid_utf8_text(voice.id, 512) ||
			    !core::valid_utf8_text(voice.name, 512) ||
			    !core::valid_utf8_text(voice.locale, 128) ||
			    (index && voices[index - 1].id == voice.id))
				reject("Invalid/duplicate speech voice discovery");
		}
		result->owner = reserve_owner();
		return speech_jobs(std::move(result));
	});
}
speech_profile speech_jobs::profile() const { return state_ ? state_->profile : speech_profile{}; }
ice::expected<speech_handle, diagnostic> speech_jobs::start(const speech_request &request) {
	return guarded([&]() -> ice::expected<speech_handle, diagnostic> {
		if (!state_)
			reject("Empty speech owner");
		state_->owner_thread();
		if (request.id.empty() || !core::valid_utf8_text(request.id, 256) || request.text.empty() ||
		    !core::valid_utf8_text(request.text, state_->limits.max_text_bytes) ||
		    !std::isfinite(request.rate_multiplier) ||
		    request.rate_multiplier < state_->profile.minimum_rate ||
		    request.rate_multiplier > state_->profile.maximum_rate ||
		    std::none_of(state_->profile.voices.begin(), state_->profile.voices.end(),
		                 [&](const auto &voice) { return voice.id == request.voice_id; }))
			reject("Invalid speech text, correlation id, installed voice or rate");
		if (state_->current) {
			std::lock_guard lock(state_->current->mutex);
			if (state_->current->phase == speech_phase::running || !state_->current->chunks.empty())
				reject("Speech job is active or has undrained PCM");
		}
		if (state_->generation == std::numeric_limits<std::uint64_t>::max())
			reject("Speech generation exhausted");
		auto candidate     = std::make_shared<speech_output::state>();
		candidate->request = request;
		candidate->limits  = state_->limits;
		state_->stop_native();
		try {
			auto started = state_->provider->start(request, speech_output(candidate));
			if (!started) {
				speech_output(candidate).fail(started.error().message, started.error().native_code);
				state_->provider->cancel();
				return ice::unexpected(started.error());
			}
		} catch (...) {
			speech_output(candidate).fail("Speech provider threw while starting");
			state_->provider->cancel();
			return ice::unexpected(diagnostic{"Speech provider threw while starting"});
		}
		state_->native_cancelled = false;
		state_->current          = std::move(candidate);
		++state_->generation;
		return speech_handle{state_->owner, state_->generation};
	});
}
ice::expected<speech_status, diagnostic> speech_jobs::inspect(speech_handle handle) const {
	return guarded([&]() -> ice::expected<speech_status, diagnostic> {
		if (!state_)
			reject("Empty speech owner");
		auto           &current = state_->get(handle);
		std::lock_guard lock(current.mutex);
		return current.snapshot();
	});
}
ice::expected<speech_update, diagnostic> speech_jobs::poll(speech_handle handle) {
	return guarded([&]() -> ice::expected<speech_update, diagnostic> {
		if (!state_)
			reject("Empty speech owner");
		auto         &current = state_->get(handle);
		speech_update result;
		{
			std::lock_guard lock(current.mutex);
			result.status = current.snapshot();
			result.chunks.reserve(
			    current.chunks.size()); // All fallible allocations precede the drain.
			for (auto &chunk : current.chunks)
				result.chunks.push_back(std::move(chunk));
			current.delivered_frames += current.buffered_frames;
			current.discard();
			result.status.delivered_frames = current.delivered_frames;
			result.status.buffered_frames  = 0;
			result.status.buffered_chunks  = 0;
		}
		if (result.status.phase == speech_phase::failed)
			state_->stop_native();
		return result;
	});
}
ice::expected<void, diagnostic> speech_jobs::cancel(speech_handle handle) {
	return guarded([&]() -> ice::expected<void, diagnostic> {
		if (!state_)
			reject("Empty speech owner");
		auto &current = state_->get(handle);
		{
			std::lock_guard lock(current.mutex);
			if (current.phase == speech_phase::running ||
			    (current.phase == speech_phase::completed && !current.chunks.empty()))
				current.phase = speech_phase::cancelled;
			current.discard();
		}
		state_->stop_native();
		return {};
	});
}
#if !defined(ICE_HAS_APPLE_SPEECH)
ice::expected<std::unique_ptr<speech_provider>, diagnostic> create_apple_speech_provider() {
	return ice::unexpected(diagnostic{"Apple system speech PCM is unavailable on this platform"});
}
#endif
} // namespace ice::audio
