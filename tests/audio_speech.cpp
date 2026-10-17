#include "ice/audio/speech.h"
#include "ice/core/utf8.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Speech %d: %s\n", __LINE__, #expression);                        \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
using namespace ice::audio;
struct record {
	std::vector<speech_output> outputs;
	std::size_t                starts = 0, cancels = 0;
	bool                       fail_start = false, throw_start = false;
};
class fake_provider final : public speech_provider {
public:
	explicit fake_provider(std::shared_ptr<record> value) : record_(std::move(value)) {
		profile_.provider = "test.owned-pcm";
		profile_.voices   = {{"test.voice", "Test system voice", "en-US"}};
	}
	const speech_profile           &profile() const noexcept override { return profile_; }
	ice::expected<void, diagnostic> start(const speech_request &, speech_output output) override {
		++record_->starts;
		record_->outputs.push_back(output);
		if (record_->throw_start)
			throw std::runtime_error("native test failure");
		if (record_->fail_start)
			return ice::unexpected(diagnostic{"native test rejection", 31});
		return {};
	}
	void cancel() noexcept override { ++record_->cancels; }

private:
	std::shared_ptr<record> record_;
	speech_profile          profile_;
};
speech_request request(std::string id = "reply:1") {
	return {std::move(id), "Hello, world.", "test.voice", 1};
}
ice::expected<speech_jobs, diagnostic> jobs(const std::shared_ptr<record> &data,
                                            speech_limits                  limits = {}) {
	return speech_jobs::create(std::make_unique<fake_provider>(data), limits);
}
} // namespace
int main() {
	using namespace ice::audio;
#if !defined(ICE_HAS_APPLE_SPEECH)
	const auto unavailable = create_apple_speech_provider();
	CHECK(!unavailable && unavailable.error().message.find("unavailable") != std::string::npos);
#endif
	CHECK(!speech_jobs::create(nullptr));
	speech_output empty;
	CHECK(!empty.active() && !empty.append(48000, 1, std::array{.1f}));
	empty.complete();
	empty.fail("stale");
	auto data    = std::make_shared<record>();
	auto service = jobs(data);
	CHECK(service);
	CHECK(service->profile().provider == "test.owned-pcm" && service->profile().voices.size() == 1);
	auto invalid     = request();
	invalid.voice_id = "missing";
	CHECK(!service->start(invalid));
	invalid      = request();
	invalid.text = std::string("x\0y", 3);
	CHECK(!service->start(invalid));
	invalid      = request();
	invalid.text = "\xc0\x80";
	CHECK(!service->start(invalid));
	invalid                 = request();
	invalid.rate_multiplier = std::numeric_limits<float>::infinity();
	CHECK(!service->start(invalid));
	CHECK(data->starts == 0);
	auto first = service->start(request());
	CHECK(first && data->starts == 1);
	CHECK(!service->start(request("busy")) && data->starts == 1);
	auto                 output = data->outputs.back();
	std::array<float, 2> input{.1f, -.2f};
	bool                 appended = false;
	std::thread          producer([&] { appended = output.append(16000, 1, input); });
	producer.join();
	CHECK(appended);
	input = {0, 0};
	CHECK(service->inspect(*first)->buffered_frames == 2);
	auto update = service->poll(*first);
	CHECK(update && update->chunks.size() == 1);
	CHECK(update->chunks[0].samples[0] == .1f && update->chunks[0].samples[1] == -.2f);
	CHECK(update->status.delivered_frames == 2 && !update->status.buffered_frames);
	CHECK(service->poll(*first)->chunks.empty());
	CHECK(output.append(16000, 1, std::array{.3f}));
	output.complete();
	output.complete();
	output.fail("must not replace completed outcome");
	CHECK(service->inspect(*first)->phase == speech_phase::completed);
	CHECK(service->inspect(*first)->buffered_frames == 1);
	CHECK(!service->start(request("not drained")) && data->starts == 1);
	CHECK(service->poll(*first)->chunks.size() == 1);
	CHECK(service->inspect(*first)->phase == speech_phase::completed);
	CHECK(service->inspect(*first)->delivered_frames == 3);
	auto second = service->start(request("reply:2"));
	CHECK(second && second->generation != first->generation);
	CHECK(!service->inspect(*first) && !service->cancel(*first));
	CHECK(!output.append(16000, 1, std::array{.1f}));
	output.fail("old callback");
	auto current = data->outputs.back();
	CHECK(current.append(48000, 2, std::array{.1f, .2f}));
	const auto cancellations = data->cancels;
	CHECK(service->cancel(*second));
	CHECK(service->cancel(*second) && data->cancels == cancellations + 1);
	CHECK(service->inspect(*second)->phase == speech_phase::cancelled);
	CHECK(service->poll(*second)->chunks.empty());
	CHECK(!current.active());
	current.complete();
	CHECK(!current.append(48000, 2, std::array{.1f, .2f}));
	CHECK(service->inspect(*second)->phase == speech_phase::cancelled);
	data->fail_start = true;
	CHECK(!service->start(request("failed")));
	CHECK(service->inspect(*second)->phase == speech_phase::cancelled);
	CHECK(!data->outputs.back().active());
	data->fail_start  = false;
	data->throw_start = true;
	CHECK(!service->start(request("threw")));
	CHECK(service->inspect(*second));
	data->throw_start = false;
	auto third        = service->start(request("reply:3"));
	CHECK(third);
	current = data->outputs.back();
	CHECK(current.append(8000, 1, std::array{.1f}));
	CHECK(!current.append(16000, 1, std::array{.2f}));
	CHECK(service->inspect(*third)->phase == speech_phase::failed);
	CHECK(service->inspect(*third)->buffered_frames == 0);
	CHECK(service->poll(*third)->chunks.empty());
	auto fourth = service->start(request("reply:4"));
	CHECK(fourth);
	CHECK(!data->outputs.back().append(48000, 1,
	                                   std::array{std::numeric_limits<float>::quiet_NaN()}));
	CHECK(service->poll(*fourth)->status.phase == speech_phase::failed);
	auto fifth = service->start(request("reply:5"));
	CHECK(fifth);
	std::string detail(510, 'x');
	detail += "é";
	data->outputs.back().fail(detail, 22);
	const auto error = service->inspect(*fifth);
	CHECK(error && error->error.native_code == 22);
	CHECK(error->error.message.size() <= 511 && ice::core::valid_utf8_text(error->error.message));
	bool        wrong_thread_rejected = false;
	std::thread wrong([&] { wrong_thread_rejected = !service->poll(*fifth); });
	wrong.join();
	CHECK(wrong_thread_rejected);
	auto moved = std::move(*service);
	CHECK(!service->inspect(*fifth) && moved.inspect(*fifth));
	auto foreign_data = std::make_shared<record>();
	auto foreign      = jobs(foreign_data);
	CHECK(foreign);
	CHECK(!foreign->inspect(*fifth));
	speech_limits small;
	small.max_text_bytes      = 4;
	small.max_buffered_bytes  = 16;
	small.max_buffered_chunks = 2;
	auto limited_data         = std::make_shared<record>();
	auto limited              = jobs(limited_data, small);
	CHECK(limited);
	auto short_request = request();
	CHECK(!limited->start(short_request) && limited_data->starts == 0);
	short_request.text = "αβ";
	auto handle        = limited->start(short_request);
	CHECK(handle);
	CHECK(limited_data->outputs.back().append(8000, 1, std::array{.1f, .2f}));
	CHECK(!limited_data->outputs.back().append(8000, 1, std::array{.1f, .2f, .3f}));
	CHECK(limited->poll(*handle)->status.phase == speech_phase::failed);
	handle = limited->start(short_request);
	CHECK(handle);
	CHECK(limited_data->outputs.back().append(8000, 1, std::array{.1f}));
	CHECK(limited_data->outputs.back().append(8000, 1, std::array{.2f}));
	CHECK(!limited_data->outputs.back().append(8000, 1, std::array{.3f}));
	CHECK(limited->poll(*handle)->chunks.empty());
	speech_limits duration;
	duration.max_duration_seconds = 1;
	auto duration_data            = std::make_shared<record>();
	auto timed                    = jobs(duration_data, duration);
	CHECK(timed);
	auto timed_handle = timed->start(request());
	CHECK(timed_handle);
	std::vector<float> half_second(4000, .1f);
	auto               timed_output = duration_data->outputs.back();
	CHECK(timed_output.append(8000, 1, half_second));
	CHECK(timed->poll(*timed_handle)->chunks.size() == 1);
	CHECK(timed_output.append(8000, 1, half_second));
	CHECK(!timed_output.append(8000, 1, std::array{.1f}));
	const auto timed_status = timed->inspect(*timed_handle);
	CHECK(timed_status);
	CHECK(timed_status->phase == speech_phase::failed && timed_status->generated_frames == 8000);
	CHECK(timed_status->delivered_frames == 4000 && !timed_status->buffered_frames);
	speech_output after_destruction;
	{
		auto temporary = jobs(std::make_shared<record>());
		CHECK(temporary);
		auto temporary_handle = temporary->start(request());
		CHECK(temporary_handle);
		// The record retains only the weak token; destruction must make its callbacks inert.
		auto owned_data = std::make_shared<record>();
		temporary       = jobs(owned_data);
		CHECK(temporary);
		CHECK(temporary->start(request()));
		after_destruction = owned_data->outputs.back();
	}
	CHECK(!after_destruction.active() && !after_destruction.append(8000, 1, std::array{.1f}));
	after_destruction.complete();
	after_destruction.fail("late");
	// A callback can race cancellation and replacement without publishing into
	// the new generation. Only the owner mutates the provider/service itself.
	auto racing_data = std::make_shared<record>();
	auto racing      = jobs(racing_data);
	CHECK(racing);
	auto old_handle = racing->start(request("racing:old"));
	CHECK(old_handle);
	const auto        old_output = racing_data->outputs.back();
	std::atomic<bool> ready{false}, replaced{false};
	bool              late_accepted = false;
	std::thread       callbacks([&] {
        ready.store(true);
        for (unsigned index = 0; index < 4096; ++index)
            old_output.append(8000, 1, std::array{.1f});
        while (!replaced.load())
            std::this_thread::yield();
        for (unsigned index = 0; index < 128; ++index)
            late_accepted = old_output.append(8000, 1, std::array{.2f}) || late_accepted;
        old_output.complete();
        old_output.fail("late failure after replacement");
    });
	while (!ready.load())
		std::this_thread::yield();
	const auto cancelled  = racing->cancel(*old_handle);
	const auto new_handle = racing->start(request("racing:new"));
	replaced.store(true);
	callbacks.join();
	CHECK(cancelled && new_handle && !late_accepted);
	CHECK(racing_data->outputs.back().append(8000, 1, std::array{.7f}));
	const auto new_update = racing->poll(*new_handle);
	CHECK(new_update && new_update->status.generated_frames == 1 && new_update->chunks.size() == 1);
	CHECK(new_update->chunks[0].samples[0] == .7f);
	std::puts("Speech jobs passed: owned async PCM, terminal/drained distinction, cancellation "
	          "generations, provider failures and budgets");
}
