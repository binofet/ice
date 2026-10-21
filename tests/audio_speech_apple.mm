#include "ice/audio/playback.h"
#include "ice/audio/speech.h"
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string_view>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Apple speech %d: %s\n", __LINE__, #expression);                  \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
int main(int argc, char **argv) {
	if (argc != 2 || std::string_view(argv[1]) != "--qualify-installed-voice") {
		std::fputs(
		    "Manual quiet qualification requires --qualify-installed-voice; no synthesis started\n",
		    stderr);
		return 1;
	}
	@autoreleasepool {
		using namespace ice::audio;
		auto provider = create_apple_speech_provider();
		if (!provider) {
			std::fprintf(stderr, "Native qualification unavailable: %s\n",
			             provider.error().message.c_str());
			return 1;
		}
		auto speech = speech_jobs::create(std::move(*provider));
		CHECK(speech);
		const auto profile = speech->profile();
		CHECK(!profile.voices.empty());
		auto selected =
		    std::find_if(profile.voices.begin(), profile.voices.end(), [](const auto &voice) {
			    return voice.locale == "en-US" && voice.name == "Samantha";
		    });
		if (selected == profile.voices.end())
			selected = std::find_if(profile.voices.begin(), profile.voices.end(),
			                        [](const auto &voice) { return voice.locale == "en-US"; });
		if (selected == profile.voices.end())
			selected = profile.voices.begin();
		std::printf("Installed native provider: %s; voice=%s; locale=%s; available=%zu\n",
		            profile.provider.c_str(), selected->id.c_str(), selected->locale.c_str(),
		            profile.voices.size());
		playback_options options;
		options.mode = playback_mode::loopback;
		options.hrtf = hrtf_policy::disabled;
		auto output  = playback_device::create(options);
		CHECK(output);
		const speech_request request{
		    "native-proof:1", "A reply is prepared without opening the speakers.", selected->id, 1};
		auto handle = speech->start(request);
		CHECK(handle);
		std::optional<voice_handle> voice;
		speech_status               final;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		for (;;) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, .01, true);
			auto update = speech->poll(*handle);
			CHECK(update);
			if (update->status.phase == speech_phase::failed) {
				std::fprintf(stderr, "Native speech failed: %s\n",
				             update->status.error.message.c_str());
				return 1;
			}
			for (const auto &chunk : update->chunks) {
				if (!voice) {
					voice_description format;
					format.sample_rate = chunk.sample_rate;
					format.channels    = chunk.channels;
					auto created       = output->create_voice(format);
					CHECK(created);
					voice = *created;
				}
				CHECK(output->queue(*voice, chunk.samples));
			}
			if (update->status.phase == speech_phase::completed) {
				final = std::move(update->status);
				break;
			}
			if (std::chrono::steady_clock::now() >= deadline) {
				CHECK(speech->cancel(*handle));
				std::fputs("Native speech qualification timed out before completion\n", stderr);
				return 1;
			}
		}
		CHECK(voice && final.generated_frames > 0 &&
		      final.generated_frames == final.delivered_frames && !final.buffered_frames);
		CHECK(output->finish(*voice) && output->play(*voice));
		const auto frames = final.generated_frames * 48000 / final.sample_rate + 4096;
		CHECK(frames <= 48000 * 30);
		std::vector<float> samples(static_cast<std::size_t>(frames) * 2);
		CHECK(output->render(samples));
		double energy = 0;
		for (const auto sample : samples) {
			CHECK(std::isfinite(sample));
			energy += double(sample) * sample;
		}
		CHECK(energy > 1e-5);
		auto cancelled = speech->start({"native-proof:2",
		                                "This generation is cancelled before its PCM is delivered.",
		                                selected->id, 1});
		CHECK(cancelled && speech->cancel(*cancelled) && speech->cancel(*cancelled));
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, .1, false);
		const auto cancelled_status = speech->inspect(*cancelled);
		CHECK(cancelled_status && cancelled_status->phase == speech_phase::cancelled &&
		      !cancelled_status->buffered_frames);
		CHECK(speech->poll(*cancelled)->chunks.empty());
		std::printf("Quiet native speech passed: frames=%llu rate=%u channels=%u "
		            "loopbackenergy=%g; no speaker output\n",
		            static_cast<unsigned long long>(final.generated_frames), final.sample_rate,
		            final.channels, energy);
	}
}
