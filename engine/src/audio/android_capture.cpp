#include "ice/audio/android_capture.h"
#include <aaudio/AAudio.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <new>
#include <optional>
#include <utility>

namespace ice::audio {
namespace {
diagnostic native_error(const char *operation, aaudio_result_t code) {
	return {std::string(operation) + ": " + AAudio_convertResultToText(code), code};
}
} // namespace
struct android_capture::implementation {
	AAudioStream                         *stream = nullptr;
	std::optional<capture_buffer>         capture;
	std::int32_t                          xruns       = 0;
	std::uint32_t                         duration_ms = 0;
	bool                                  started     = false;
	std::chrono::steady_clock::time_point deadline;
	~implementation() {
		if (stream) {
			if (started)
				AAudioStream_requestStop(stream);
			AAudioStream_close(stream);
		}
	}
};
android_capture::android_capture()  = default;
android_capture::~android_capture() = default;
ice::expected<void, diagnostic> android_capture::start(std::uint32_t duration_ms) {
	auto prepared = prepare(duration_ms);
	return prepared ? start_prepared() : prepared;
}
ice::expected<void, diagnostic> android_capture::prepare(std::uint32_t duration_ms) {
	if (stop_failure_)
		return ice::unexpected(
		    native_error("Prior microphone stop/close was not confirmed", stop_failure_));
	if (state_ && state_->stream)
		return ice::unexpected(diagnostic{"Microphone capture is already active"});
	if (!duration_ms || duration_ms > 10000)
		return ice::unexpected(diagnostic{"Microphone capture requires 1..10000 ms"});
	try {
		auto                 candidate = std::make_unique<implementation>();
		AAudioStreamBuilder *builder   = nullptr;
		auto                 result    = AAudio_createStreamBuilder(&builder);
		if (result != AAUDIO_OK)
			return ice::unexpected(native_error("Create microphone stream builder", result));
		AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
		AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
		AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
		AAudioStreamBuilder_setChannelCount(builder, 1);
		AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
		AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_RECOGNITION);
		result = AAudioStreamBuilder_openStream(builder, &candidate->stream);
		AAudioStreamBuilder_delete(builder);
		if (result != AAUDIO_OK)
			return ice::unexpected(native_error("Open microphone input", result));
		const auto rate = AAudioStream_getSampleRate(candidate->stream);
		if (AAudioStream_getChannelCount(candidate->stream) != 1 ||
		    AAudioStream_getFormat(candidate->stream) != AAUDIO_FORMAT_PCM_I16 || rate < 8000 ||
		    rate > 192000)
			return ice::unexpected(diagnostic{"Microphone negotiated unsupported PCM format/rate"});
		auto buffer = capture_buffer::create(static_cast<std::uint32_t>(rate), duration_ms);
		if (!buffer)
			return ice::unexpected(buffer.error());
		candidate->capture.emplace(std::move(*buffer));
		candidate->duration_ms = duration_ms;
		state_                 = std::move(candidate);
		return {};
	} catch (const std::bad_alloc &) {
		return ice::unexpected(diagnostic{"Cannot allocate microphone capture owner"});
	}
}
ice::expected<void, diagnostic> android_capture::start_prepared() {
	if (!state_ || !state_->stream || !state_->capture || state_->started)
		return ice::unexpected(diagnostic{"Microphone input is not prepared for start"});
	const auto result = AAudioStream_requestStart(state_->stream);
	if (result != AAUDIO_OK) {
		auto cleanup = stop();
		auto failure = native_error("Start microphone input", result);
		if (!cleanup)
			failure.message += "; cleanup: " + cleanup.error().message;
		return ice::unexpected(std::move(failure));
	}
	state_->started = true;
	state_->deadline =
	    std::chrono::steady_clock::now() + std::chrono::milliseconds(state_->duration_ms);
	return {};
}
ice::expected<void, diagnostic> android_capture::poll() {
	if (!recording())
		return {};
	if (std::chrono::steady_clock::now() >= state_->deadline)
		return stop();
	// getState only returns cached client state. A zero-timeout UNKNOWN query
	// explicitly pumps service events, or STARTING can persist forever without reads.
	aaudio_stream_state_t stream_state = AAUDIO_STREAM_STATE_UNKNOWN;
	const auto            advanced     = AAudioStream_waitForStateChange(
        state_->stream, AAUDIO_STREAM_STATE_UNKNOWN, &stream_state, 0);
	if (advanced != AAUDIO_OK && advanced != AAUDIO_ERROR_TIMEOUT) {
		(void)stop();
		return ice::unexpected(native_error("Update microphone stream state", advanced));
	}
	if (stream_state == AAUDIO_STREAM_STATE_OPEN || stream_state == AAUDIO_STREAM_STATE_STARTING)
		return {}; // requestStart is asynchronous; never block XR waiting for it.
	if (stream_state != AAUDIO_STREAM_STATE_STARTED) {
		(void)stop();
		return ice::unexpected(
		    native_error("Microphone stream left started state", AAUDIO_ERROR_DISCONNECTED));
	}
	std::array<std::int16_t, 2048> samples{};
	for (unsigned chunk = 0; chunk < 8; ++chunk) {
		const auto frames = AAudioStream_read(state_->stream, samples.data(),
		                                      static_cast<std::int32_t>(samples.size()), 0);
		if (frames < 0 || frames > static_cast<std::int32_t>(samples.size())) {
			(void)stop();
			return ice::unexpected(native_error("Read microphone input",
			                                    frames < 0 ? frames : AAUDIO_ERROR_OUT_OF_RANGE));
		}
		state_->xruns = std::max(0, AAudioStream_getXRunCount(state_->stream));
		if (!frames)
			break;
		state_->capture->append({samples.data(), static_cast<std::size_t>(frames)});
		if (state_->capture->full())
			return stop();
		if (frames < static_cast<std::int32_t>(samples.size()))
			break;
	}
	return {};
}
ice::expected<void, diagnostic> android_capture::stop() {
	if (stop_failure_)
		return ice::unexpected(
		    native_error("Prior microphone stop/close was not confirmed", stop_failure_));
	if (!state_ || !state_->stream)
		return {};
	auto *stream       = state_->stream;
	state_->stream     = nullptr;
	const auto stopped = state_->started ? AAudioStream_requestStop(stream) : AAUDIO_OK;
	state_->started    = false;
	const auto closed  = AAudioStream_close(stream);
	if (closed != AAUDIO_OK) {
		stop_failure_ = closed;
		return ice::unexpected(native_error("Close microphone input", closed));
	}
	if (stopped != AAUDIO_OK && stopped != AAUDIO_ERROR_DISCONNECTED &&
	    stopped != AAUDIO_ERROR_INVALID_STATE) {
		stop_failure_ = stopped;
		return ice::unexpected(native_error("Stop microphone input", stopped));
	}
	return {};
}
void android_capture::discard() {
	(void)stop();
	if (state_ && state_->capture)
		state_->capture->reset();
	state_.reset();
}
bool android_capture::recording() const { return state_ && state_->stream && state_->started; }
const capture_buffer *android_capture::buffer() const {
	return state_ && state_->capture ? &*state_->capture : nullptr;
}
std::int32_t android_capture::xruns() const { return state_ ? state_->xruns : 0; }
} // namespace ice::audio
