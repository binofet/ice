#include "ice/audio/speech.h"
#include "ice/core/utf8.h"
#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>
#include <algorithm>
#include <array>
#include <cmath>

@interface ice_speech_delegate : NSObject <AVSpeechSynthesizerDelegate> {
@public
	ice::audio::speech_output output_;
}
@end
@implementation ice_speech_delegate
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer
    didCancelSpeechUtterance:(AVSpeechUtterance *)utterance {
	(void)synthesizer;
	(void)utterance;
	output_.fail("Apple speech synthesis was cancelled by the native provider");
}
@end

namespace ice::audio {
namespace {
std::string native_text(NSString *value) {
	NSData *data = [value dataUsingEncoding:NSUTF8StringEncoding];
	if (!data || !data.length || data.length > 512)
		return {};
	return std::string(static_cast<const char *>(data.bytes), data.length);
}
NSString *native_string(std::string_view value) {
	return [[NSString alloc] initWithBytes:value.data()
	                                length:value.size()
	                              encoding:NSUTF8StringEncoding];
}
void receive(AVAudioBuffer *value, const speech_output &output) noexcept {
	@autoreleasepool {
		@try {
			if (!output.active())
				return;
			if (![value isKindOfClass:[AVAudioPCMBuffer class]]) {
				output.fail("Apple speech returned a non-PCM buffer");
				return;
			}
			AVAudioPCMBuffer *buffer = static_cast<AVAudioPCMBuffer *>(value);
			if (!buffer.frameLength) {
				output.complete();
				return;
			}
			const auto frames   = buffer.frameLength;
			const auto channels = buffer.format.channelCount;
			const auto rate     = buffer.format.sampleRate;
			const auto format   = buffer.format.commonFormat;
			if (frames > buffer.frameCapacity || (channels != 1 && channels != 2) ||
			    !std::isfinite(rate) || rate < 8000 || rate > 192000 || std::floor(rate) != rate) {
				output.fail("Apple speech returned an unsupported PCM format");
				return;
			}
			std::size_t sample_bytes = 0;
			switch (format) {
			case AVAudioPCMFormatFloat32:
				sample_bytes = sizeof(float);
				break;
			case AVAudioPCMFormatFloat64:
				sample_bytes = sizeof(double);
				break;
			case AVAudioPCMFormatInt16:
				sample_bytes = sizeof(std::int16_t);
				break;
			case AVAudioPCMFormatInt32:
				sample_bytes = sizeof(std::int32_t);
				break;
			default:
				output.fail("Apple speech returned unsupported PCM sample storage");
				return;
			}
			const bool  interleaved = buffer.format.isInterleaved;
			const auto *list        = buffer.audioBufferList;
			if (!list || list->mNumberBuffers != (interleaved ? 1 : channels)) {
				output.fail("Apple speech PCM buffer layout is inconsistent");
				return;
			}
			for (std::uint32_t channel = 0; channel < list->mNumberBuffers; ++channel) {
				const auto &data  = list->mBuffers[channel];
				const auto  count = interleaved ? channels : 1u;
				if (data.mNumberChannels != count || !data.mData ||
				    std::uint64_t(frames) * count * sample_bytes > data.mDataByteSize) {
					output.fail("Apple speech PCM buffer storage is truncated");
					return;
				}
			}
			// Fixed scratch storage bounds conversion work per append. No playback,
			// Objective-C client or application callback is invoked from this block.
			std::array<float, 4096> scratch{};
			for (std::uint32_t offset = 0; offset < frames;) {
				const auto count = std::min<std::uint32_t>(frames - offset, 2048);
				for (std::uint32_t frame = 0; frame < count; ++frame) {
					for (std::uint32_t channel = 0; channel < channels; ++channel) {
						const auto *data = list->mBuffers[interleaved ? 0 : channel].mData;
						const auto  index =
						    std::size_t(offset + frame) * (interleaved ? channels : 1) +
						    (interleaved ? channel : 0);
						float sample = 0;
						switch (format) {
						case AVAudioPCMFormatFloat32:
							sample = static_cast<const float *>(data)[index];
							break;
						case AVAudioPCMFormatFloat64: {
							const auto value = static_cast<const double *>(data)[index];
							if (!std::isfinite(value) || value < -1 || value > 1) {
								output.fail("Apple speech PCM must be finite and normalized");
								return;
							}
							sample = static_cast<float>(value);
							break;
						}
						case AVAudioPCMFormatInt16:
							sample =
							    float(static_cast<const std::int16_t *>(data)[index]) / 32768.f;
							break;
						case AVAudioPCMFormatInt32:
							sample = static_cast<float>(
							    double(static_cast<const std::int32_t *>(data)[index]) /
							    2147483648.0);
							break;
						default:
							return;
						}
						scratch[std::size_t(frame) * channels + channel] = sample;
					}
				}
				if (!output.append(static_cast<std::uint32_t>(rate), channels,
				                   std::span(scratch).first(std::size_t(count) * channels)))
					return;
				offset += count;
			}
		} @catch (NSException *error) {
			(void)error;
			output.fail("Apple speech native PCM callback failed");
		}
	}
}
class apple_speech_provider final : public speech_provider {
public:
	explicit apple_speech_provider(speech_profile profile) : profile_(std::move(profile)) {}
	~apple_speech_provider() override { cancel(); }
	const speech_profile           &profile() const noexcept override { return profile_; }
	ice::expected<void, diagnostic> start(const speech_request &request,
	                                      speech_output         output) override {
		if (![NSThread isMainThread])
			return ice::unexpected(diagnostic{"Apple speech requires the main owner thread"});
		if (!core::valid_utf8_text(request.text, 16u * 1024u * 1024u) || request.text.empty() ||
		    !std::isfinite(request.rate_multiplier) ||
		    request.rate_multiplier < profile_.minimum_rate ||
		    request.rate_multiplier > profile_.maximum_rate ||
		    std::none_of(profile_.voices.begin(), profile_.voices.end(),
		                 [&](const auto &voice) { return voice.id == request.voice_id; }))
			return ice::unexpected(diagnostic{"Invalid installed Apple speech request"});
		@autoreleasepool {
			@try {
				if (@available(macOS 10.15, iOS 13, tvOS 13, watchOS 6, *)) {
					AVSpeechSynthesisVoice *voice = [AVSpeechSynthesisVoice
					    voiceWithIdentifier:native_string(request.voice_id)];
					if (!voice || ![voice.identifier hasPrefix:@"com.apple."])
						return ice::unexpected(
						    diagnostic{"Requested Apple voice is no longer installed"});
					cancel();
					synth_             = [[AVSpeechSynthesizer alloc] init];
					delegate_          = [[ice_speech_delegate alloc] init];
					delegate_->output_ = output;
					synth_.delegate    = delegate_;
					AVSpeechUtterance *utterance =
					    [AVSpeechUtterance speechUtteranceWithString:native_string(request.text)];
					utterance.voice = voice;
					utterance.rate  = AVSpeechUtteranceDefaultSpeechRate * request.rate_multiplier;
					[synth_ writeUtterance:utterance
					      toBufferCallback:^(AVAudioBuffer *buffer) {
						    receive(buffer, output);
					      }];
					return {};
				}
				return ice::unexpected(
				    diagnostic{"Apple speech PCM requires a newer operating system"});
			} @catch (NSException *error) {
				(void)error;
				cancel();
				return ice::unexpected(diagnostic{"Apple speech native preparation failed"});
			}
		}
	}
	void cancel() noexcept override {
		@try {
			synth_.delegate = nil;
			[synth_ stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
			synth_    = nil;
			delegate_ = nil;
		} @catch (NSException *error) {
			(void)error;
		}
	}

private:
	speech_profile                profile_;
	AVSpeechSynthesizer *__strong synth_    = nil;
	ice_speech_delegate *__strong delegate_ = nil;
};
} // namespace
ice::expected<std::unique_ptr<speech_provider>, diagnostic> create_apple_speech_provider() {
	if (![NSThread isMainThread])
		return ice::unexpected(diagnostic{"Apple speech discovery requires the main owner thread"});
	try {
		@autoreleasepool {
			@try {
				speech_profile profile;
				profile.provider = "apple.avspeech.installed-system";
				profile.maximum_rate =
				    AVSpeechUtteranceMaximumSpeechRate / AVSpeechUtteranceDefaultSpeechRate;
				for (AVSpeechSynthesisVoice *voice in [AVSpeechSynthesisVoice speechVoices]) {
					if (![voice.identifier hasPrefix:@"com.apple."] ||
					    ![AVSpeechSynthesisVoice voiceWithIdentifier:voice.identifier])
						continue;
					if (@available(macOS 14, iOS 17, tvOS 17, watchOS 10, *))
						if (voice.voiceTraits & AVSpeechSynthesisVoiceTraitIsPersonalVoice)
							continue;
					profile.voices.push_back({native_text(voice.identifier),
					                          native_text(voice.name),
					                          native_text(voice.language)});
				}
				if (profile.voices.empty())
					return ice::unexpected(
					    diagnostic{"No installed Apple system speech voices are available"});
				return std::unique_ptr<speech_provider>(
				    std::make_unique<apple_speech_provider>(std::move(profile)));
			} @catch (NSException *error) {
				(void)error;
				return ice::unexpected(diagnostic{"Apple system speech discovery failed"});
			}
		}
	} catch (...) {
		return ice::unexpected(diagnostic{"Apple speech discovery allocation failed"});
	}
}
} // namespace ice::audio
