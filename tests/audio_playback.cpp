#include "ice/audio/playback.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <thread>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Playback %d: %s\n", __LINE__, #expression);                      \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
using namespace ice::audio;
std::vector<float> sine(std::size_t frames, float frequency = 440) {
	std::vector<float> values(frames);
	for (std::size_t index = 0; index < frames; ++index)
		values[index] = .2f * std::sin(float(index) * frequency * 6.28318530718f / 48000.f);
	return values;
}
double energy(std::span<const float> samples, std::size_t channel = 0, std::size_t stride = 1) {
	double result = 0;
	for (std::size_t index = channel; index < samples.size(); index += stride)
		result += double(samples[index]) * samples[index];
	return result;
}
ice::expected<playback_device, diagnostic> offline(hrtf_policy hrtf = hrtf_policy::disabled) {
	playback_options options;
	options.mode = playback_mode::loopback;
	options.hrtf = hrtf;
	return playback_device::create(options);
}
ice::expected<std::vector<float>, diagnostic>
sound(voice_parameters parameters, std::span<const float> input, std::size_t frames = 12000) {
	auto device = offline();
	if (!device)
		return ice::unexpected(device.error());
	voice_description description;
	description.parameters = parameters;
	auto handle            = device->create_voice(description);
	if (!handle)
		return ice::unexpected(handle.error());
	if (auto result = device->queue(*handle, input); !result)
		return ice::unexpected(result.error());
	if (auto result = device->finish(*handle); !result)
		return ice::unexpected(result.error());
	if (auto result = device->play(*handle); !result)
		return ice::unexpected(result.error());
	std::vector<float> result(frames * 2);
	if (auto rendered = device->render(result); !rendered)
		return ice::unexpected(rendered.error());
	return result;
}
} // namespace
int main() {
	using namespace ice::audio;
	auto device = offline();
	if (!device)
		std::fprintf(stderr, "%s native%d\n", device.error().message.c_str(),
		             device.error().native_code);
	CHECK(device);
	const auto profile = device->capabilities();
	std::printf(
	    "Actual playback: %s; %s; rate%u; loopback%d float%d EFX%d HRTFavailable%d active%d\n",
	    profile.renderer.c_str(), profile.output.c_str(), profile.sample_rate, profile.loopback,
	    profile.float_pcm, profile.effects, profile.hrtf, profile.hrtf_active);
	CHECK(profile.loopback && profile.float_pcm && profile.effects && profile.hrtf &&
	      !profile.hrtf_active);
	CHECK(profile.sample_rate == 48000);
	auto source = device->create_voice();
	CHECK(source);
	CHECK(device->play(*source));
	CHECK(device->inspect(*source)->phase == voice_phase::starved);
	CHECK(device->inspect(*source)->starvation_count == 1);
	auto pcm = sine(2048);
	CHECK(device->queue(*source, pcm));
	std::fill(pcm.begin(), pcm.end(), 0.f); // Native upload must own the original bytes.
	CHECK(device->poll());
	CHECK(device->inspect(*source)->phase == voice_phase::playing);
	std::vector<float> output(1024 * 2);
	CHECK(device->render(output));
	CHECK(energy(output) > 1);
	CHECK(device->pause(*source));
	CHECK(device->inspect(*source)->phase == voice_phase::paused);
	CHECK(device->suspend());
	std::fill(output.begin(), output.end(), 99.f);
	CHECK(device->render(output));
	CHECK(energy(output) == 0);
	CHECK(device->resume());
	CHECK(device->inspect(*source)->phase == voice_phase::paused);
	CHECK(device->play(*source));
	CHECK(device->finish(*source));
	CHECK(!device->queue(*source, std::array{.1f}));
	output.resize(4096 * 2);
	CHECK(device->render(output));
	CHECK(device->inspect(*source)->phase == voice_phase::completed);
	CHECK(device->inspect(*source)->queued_blocks == 0);
	CHECK(device->stop(*source));
	CHECK(device->queue(*source, sine(1024)));
	CHECK(device->play(*source));
	CHECK(device->suspend());
	CHECK(device->inspect(*source)->phase == voice_phase::paused);
	CHECK(device->resume());
	CHECK(device->inspect(*source)->phase == voice_phase::playing);
	output.resize(2048 * 2);
	CHECK(device->render(output));
	CHECK(device->inspect(*source)->phase == voice_phase::starved);
	CHECK(device->queue(*source, sine(1024)));
	CHECK(device->poll());
	CHECK(device->inspect(*source)->phase == voice_phase::playing);
	CHECK(device->render(output));
	CHECK(energy(output) > 1);
	CHECK(device->retire(*source));
	CHECK(!device->inspect(*source));
	auto replacement = device->create_voice();
	CHECK(replacement);
	CHECK(replacement->slot == source->slot && replacement->generation != source->generation);
	auto other = offline();
	CHECK(other);
	CHECK(!other->play(*replacement));
	bool        wrong_thread_rejected = false;
	std::thread thread([&] { wrong_thread_rejected = !device->poll(); });
	thread.join();
	CHECK(wrong_thread_rejected);
	auto moved = std::move(*device);
	CHECK(!device->poll());
	CHECK(moved.inspect(*replacement));
	listener_pose invalid;
	invalid.forward = {0, 1, 0};
	invalid.up      = {0, 1, 0};
	CHECK(!moved.set_listener(invalid));
	CHECK(moved.set_listener({}));
	listener_pose wide;
	wide.forward = {0, 0, -std::numeric_limits<float>::max()};
	wide.up      = {0, std::numeric_limits<float>::max(), 0};
	CHECK(moved.set_listener(wide));
	wide.up = wide.forward;
	CHECK(!moved.set_listener(wide));
	wide.forward = {0, 0, -std::numeric_limits<float>::min()};
	wide.up      = {0, std::numeric_limits<float>::min(), 0};
	CHECK(moved.set_listener(wide));
	invalid.position.x = std::numeric_limits<float>::quiet_NaN();
	CHECK(!moved.set_listener(invalid));
	std::array<float, 3> odd{4, 5, 6};
	CHECK(!moved.render(odd));
	CHECK(odd[0] == 4);
	voice_description stereo;
	stereo.channels           = 2;
	stereo.parameters.spatial = true;
	CHECK(!moved.create_voice(stereo));
	voice_parameters invalid_source;
	invalid_source.pitch = 0;
	CHECK(!moved.configure(*replacement, invalid_source));
	CHECK(!moved.queue(*replacement, std::array{std::numeric_limits<float>::infinity()}));
	playback_options limits;
	limits.mode   = playback_mode::loopback;
	limits.hrtf   = hrtf_policy::disabled;
	limits.limits = {2, 4096, 2};
	auto bounded  = playback_device::create(limits);
	CHECK(bounded);
	auto one = bounded->create_voice(), two = bounded->create_voice();
	CHECK(one && two);
	CHECK(!bounded->create_voice());
	CHECK(bounded->queue(*one, sine(512)));
	CHECK(bounded->queue(*two, sine(512)));
	CHECK(!bounded->queue(*one, std::array{.1f}));
	CHECK(bounded->inspect(*one)->queued_frames == 512);
	CHECK(bounded->stop(*two));
	CHECK(bounded->queue(*one, sine(512)));
	CHECK(bounded->inspect(*one)->queued_frames == 1024);
	voice_parameters left;
	left.spatial          = true;
	left.position         = {-2, 0, -1};
	auto right            = left;
	right.position.x      = 2;
	const auto tone       = sine(8192);
	auto       left_audio = sound(left, tone), right_audio = sound(right, tone);
	CHECK(left_audio && right_audio);
	CHECK(energy(*left_audio, 0, 2) > energy(*left_audio, 1, 2) * 2);
	CHECK(energy(*right_audio, 1, 2) > energy(*right_audio, 0, 2) * 2);
	voice_parameters near;
	near.spatial    = true;
	near.position.z = -1;
	auto far        = near;
	far.position.z  = -8;
	auto near_audio = sound(near, tone), far_audio = sound(far, tone);
	CHECK(near_audio && far_audio);
	CHECK(energy(*near_audio) > energy(*far_audio) * 8);
	voice_parameters dry;
	auto             filtered    = dry;
	filtered.high_frequency_gain = .01f;
	auto high                    = sine(8192, 10000);
	auto dry_audio = sound(dry, high), filtered_audio = sound(filtered, high);
	CHECK(dry_audio && filtered_audio);
	CHECK(energy(*dry_audio) > energy(*filtered_audio) * 4);
	std::vector<float> impulse(1024, 0.f);
	impulse[0]       = 1;
	auto wet         = dry;
	wet.reverb_send  = 1;
	auto dry_impulse = sound(dry, impulse, 24000), wet_impulse = sound(wet, impulse, 24000);
	CHECK(dry_impulse && wet_impulse);
	CHECK(energy(std::span(*wet_impulse).subspan(8192)) >
	      energy(std::span(*dry_impulse).subspan(8192)) + 1e-8);
	auto hrtf = offline(hrtf_policy::require);
	CHECK(hrtf);
	CHECK(hrtf->capabilities().hrtf_active);
	CHECK(!hrtf->capabilities().hrtf_name.empty());
	std::printf("Actual active HRTF dataset: %s\n", hrtf->capabilities().hrtf_name.c_str());
	voice_description spatial;
	spatial.parameters = left;
	auto hrtf_voice    = hrtf->create_voice(spatial);
	CHECK(hrtf_voice);
	CHECK(hrtf->queue(*hrtf_voice, tone));
	CHECK(hrtf->play(*hrtf_voice));
	output.resize(8192 * 2);
	CHECK(hrtf->render(output));
	CHECK(energy(output) > 1e-5);
	std::puts("Playback passed: real loopback/EFX/HRTF, owned queues, spatial attenuation, "
	          "lifecycle and generations");
}
