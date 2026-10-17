#include "ice/audio/android/speech.h"
#include "stream.h"
#include <bit>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace ice::audio;
using android_detail::encoding;
#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression))                                                                         \
			throw std::runtime_error("Android speech check " + std::to_string(__LINE__) +          \
			                         ": " #expression);                                            \
	} while (false)
struct fixture_provider final : speech_provider {
	std::shared_ptr<android_detail::stream> stream = std::make_shared<android_detail::stream>();
	speech_profile discovery{"synthetic_android_bridge", {{"fixture", "Fixture", "en"}}, .25f, 2.f};
	std::uint64_t  generation = 0;
	const speech_profile           &profile() const noexcept override { return discovery; }
	ice::expected<void, diagnostic> start(const speech_request &, speech_output output) override {
		auto next = stream->start(std::move(output));
		if (!next)
			return ice::unexpected(next.error());
		generation = *next;
		return {};
	}
	void cancel() noexcept override { stream->cancel(); }
};
struct fixture {
	fixture_provider *provider = nullptr;
	speech_jobs       jobs;
	speech_handle     handle;
	fixture() : jobs(make()) { start("one"); }
	speech_jobs make() {
		auto owned = std::make_unique<fixture_provider>();
		provider   = owned.get();
		auto value = speech_jobs::create(std::move(owned));
		CHECK(value);
		return std::move(*value);
	}
	void start(std::string id) {
		auto value = jobs.start({std::move(id), "Synthetic sample", "fixture", 1});
		CHECK(value);
		handle = *value;
	}
	bool begin(encoding format, std::uint32_t channels = 1) {
		return provider->stream->begin(provider->generation, 48000, format, channels);
	}
	bool audio(std::span<const std::byte> bytes) {
		return provider->stream->audio(provider->generation, bytes);
	}
	bool done(bool final = true) { return provider->stream->done(provider->generation, final); }
	speech_update poll() {
		auto result = jobs.poll(handle);
		CHECK(result);
		return std::move(*result);
	}
};
std::vector<float> samples(speech_update update) {
	std::vector<float> result;
	for (auto &chunk : update.chunks)
		result.insert(result.end(), chunk.samples.begin(), chunk.samples.end());
	return result;
}
void unicode_segmentation() {
	auto split = android_detail::segments("A🐇é\nB", 3);
	CHECK(split && split->size() == 2);
	std::u16string joined;
	for (const auto &part : *split) {
		CHECK(!part.empty() && part.size() <= 3);
		CHECK(!(part.front() >= 0xdc00 && part.front() <= 0xdfff));
		CHECK(!(part.back() >= 0xd800 && part.back() <= 0xdbff));
		joined += part;
	}
	CHECK(joined == u"A🐇é\nB");
	CHECK(!android_detail::segments("🐇", 1));
	CHECK(!android_detail::segments(std::string("bad\0text", 8), 4000));
	auto long_text = android_detail::segments(std::string(1024u * 1024u, 'a'), 4000);
	CHECK(long_text && long_text->size() == 263);
	std::size_t count = 0;
	for (const auto &part : *long_text)
		count += part.size();
	CHECK(count == 1024u * 1024u);
}
void formats_and_carry() {
	fixture f;
	CHECK(f.begin(encoding::pcm_s16, 2));
	const std::array<std::byte, 8> data{std::byte{0},    std::byte{0x80}, std::byte{0xff},
	                                    std::byte{0x7f}, std::byte{0},    std::byte{0},
	                                    std::byte{0},    std::byte{0x40}};
	CHECK(f.audio(std::span(data).first(3)));
	CHECK(f.poll().chunks.empty());
	CHECK(f.audio(std::span(data).subspan(3, 2)));
	CHECK(f.audio(std::span(data).subspan(5)));
	CHECK(f.done(false));
	auto update = f.poll();
	CHECK(update.status.phase == speech_phase::running);
	CHECK(samples(std::move(update)) == std::vector<float>({-1, 32767.f / 32768.f, 0, .5f}));
	// Internal native subutterances retain one logical format and do not complete the job early.
	CHECK(f.begin(encoding::pcm_s16, 2));
	CHECK(f.audio(data));
	CHECK(f.done());
	update = f.poll();
	CHECK(update.status.phase == speech_phase::completed && update.status.generated_frames == 4);
	CHECK(f.poll().chunks.empty());
	fixture eight;
	CHECK(eight.begin(encoding::pcm_u8));
	const std::array<std::byte, 3> bytes{std::byte{0}, std::byte{128}, std::byte{255}};
	CHECK(eight.audio(bytes) && eight.done());
	CHECK(samples(eight.poll()) == std::vector<float>({-1, 0, 127.f / 128.f}));
	fixture floating;
	CHECK(floating.begin(encoding::pcm_f32));
	std::array<std::byte, 12>  floats{};
	const std::array<float, 3> values{-1, .25f, 1};
	for (std::size_t i = 0; i < values.size(); ++i) {
		const auto bits = std::bit_cast<std::uint32_t>(values[i]);
		for (unsigned n = 0; n < 4; ++n)
			floats[i * 4 + n] = std::byte((bits >> (n * 8)) & 255);
	}
	CHECK(floating.audio(floats) && floating.done());
	CHECK(samples(floating.poll()) == std::vector<float>({-1, .25f, 1}));
}
void failures_and_generations() {
	fixture incomplete;
	CHECK(incomplete.begin(encoding::pcm_s16, 2));
	CHECK(incomplete.audio(std::array{std::byte{1}}));
	CHECK(!incomplete.done());
	CHECK(incomplete.poll().status.phase == speech_phase::failed);
	fixture changing;
	CHECK(changing.begin(encoding::pcm_u8));
	CHECK(changing.audio(std::array{std::byte{128}}));
	CHECK(!changing.begin(encoding::pcm_f32));
	auto failed = changing.poll();
	CHECK(failed.status.phase == speech_phase::failed && failed.chunks.empty());
	fixture unknown;
	CHECK(!unknown.begin(static_cast<encoding>(42)));
	CHECK(unknown.poll().status.phase == speech_phase::failed);
	fixture nan;
	CHECK(nan.begin(encoding::pcm_f32));
	std::array<std::byte, 4> data{};
	const auto bits = std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN());
	for (unsigned n = 0; n < 4; ++n)
		data[n] = std::byte((bits >> (n * 8)) & 255);
	CHECK(!nan.audio(data));
	CHECK(nan.poll().status.phase == speech_phase::failed);
	fixture f;
	CHECK(f.begin(encoding::pcm_u8));
	const auto old = f.provider->generation;
	CHECK(f.jobs.cancel(f.handle));
	f.start("replacement");
	CHECK(f.provider->generation != old);
	CHECK(!f.provider->stream->begin(old, 48000, encoding::pcm_u8, 1));
	f.provider->stream->fail(old, "Old error", -1);
	CHECK(f.begin(encoding::pcm_u8));
	CHECK(f.audio(std::array{std::byte{128}}) && f.done());
	CHECK(f.poll().status.phase == speech_phase::completed);
}
void late_threaded_callback() {
	fixture f;
	CHECK(f.begin(encoding::pcm_u8));
	auto                    stream = f.provider->stream;
	const auto              old    = f.provider->generation;
	std::mutex              mutex;
	std::condition_variable changed;
	bool                    first = false, replaced = false, late_accepted = true;
	std::thread             callback([&] {
        CHECK(stream->audio(old, std::array{std::byte{128}}));
        {
            std::lock_guard lock(mutex);
            first = true;
        }
        changed.notify_one();
        {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return replaced; });
        }
        late_accepted = stream->audio(old, std::array{std::byte{255}});
        stream->done(old, true);
    });
	{
		std::unique_lock lock(mutex);
		changed.wait(lock, [&] { return first; });
	}
	CHECK(f.jobs.cancel(f.handle));
	f.start("thread-replacement");
	{
		std::lock_guard lock(mutex);
		replaced = true;
	}
	changed.notify_one();
	callback.join();
	CHECK(!late_accepted);
	CHECK(f.begin(encoding::pcm_u8));
	CHECK(f.audio(std::array{std::byte{0}}) && f.done());
	CHECK(samples(f.poll()) == std::vector<float>({-1}));
}
} // namespace
int main() {
	try {
#if defined(__ANDROID__)
		// Force a real public-factory consumer cross-link, without starting Java or an engine.
		CHECK(!android_speech_preparation::begin(nullptr, nullptr));
#endif
		unicode_segmentation();
		formats_and_carry();
		failures_and_generations();
		late_threaded_callback();
		std::puts("Android speech portable core: exact Unicode segments, PCM formats/carry, "
		          "terminal and late-generation tests passed");
	} catch (const std::exception &e) {
		std::fprintf(stderr, "%s\n", e.what());
		return 1;
	}
}
