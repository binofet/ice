#include "ice/audio/capture_intent.h"
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(expression)                                                                          \
	do {                                                                                           \
		if (!(expression)) {                                                                       \
			std::fprintf(stderr, "Capture intent failed %d: %s\n", __LINE__, #expression);         \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

namespace {
using ice::audio::diagnostic;
using result = ice::expected<void, diagnostic>;
enum class failure { none, prepare, before, start, stop, ending };
struct fake_owner {
	bool                     stop_uncertain = false, start_closes_then_fails = false;
	std::vector<std::string> calls;
	std::optional<ice::audio::capture_buffer> pcm;
	failure                                   reject = failure::none, throwing = failure::none;
	bool                                      recording = false, speaking = true;
	std::uint64_t                             generation = 1;
	unsigned                                  after      = 0;
	std::vector<bool>                         native_stop_confirmed;
	result                                    step(failure stage, const char *name) {
        calls.emplace_back(name);
        if (throwing == stage)
            throw std::runtime_error("Fake native/observer failure");
        if (reject == stage)
            return ice::unexpected(diagnostic{std::string(name) + " rejected", 42});
        return {};
	}
	result stop_native() {
		recording = false;
		pcm.reset();
		const bool prior = stop_uncertain;
		if (reject == failure::stop || throwing == failure::stop)
			stop_uncertain = true;
		auto result = step(failure::stop, "microphone_stop");
		if (!result)
			return result;
		if (prior)
			return ice::unexpected(diagnostic{"Prior native cleanup remains uncertain", 42});
		return {};
	}
	ice::audio::capture_intent_callbacks callbacks() {
		return {
		    [this]() -> result {
			    auto buffer = ice::audio::capture_buffer::create(16000, 100);
			    if (!buffer)
				    return ice::unexpected(buffer.error());
			    pcm.emplace(std::move(*buffer));
			    return step(failure::prepare, "prepare");
		    },
		    [this]() -> result {
			    if (!pcm || pcm->capacity_frames() != 1600)
				    return ice::unexpected(diagnostic{"Observer ran before PCM preparation"});
			    speaking = false;
			    ++generation; // Retire asynchronous synthesis before any native microphone start.
			    return step(failure::before, "cancel_output");
		    },
		    [this]() -> result {
			    if (speaking || !pcm)
				    return ice::unexpected(diagnostic{"Native start preceded output cancellation"});
			    recording   = true; // A failed native start may have partially acquired resources.
			    auto result = step(failure::start, "microphone_start");
			    if (result && start_closes_then_fails) {
				    (void)stop_native(); // Native startup may already have attempted cleanup.
				    return ice::unexpected(diagnostic{"Native startup rejected", 7});
			    }
			    return result;
		    },
		    [this] { return stop_native(); },
		    [this](bool confirmed) {
			    ++after;
			    native_stop_confirmed.push_back(confirmed);
			    if (recording || pcm)
				    throw std::runtime_error("Ending notification preceded native cleanup");
			    (void)step(failure::ending, "ending");
		    }};
	}
	bool accept_late_synthesis(std::uint64_t token) const {
		return token == generation && speaking;
	}
};
} // namespace

int main() {
	using ice::audio::capture_intent;
	{
		fake_owner     owner;
		capture_intent intent(owner.callbacks());
		const auto     prior_generation = owner.generation;
		CHECK(intent.begin() && intent.active() && owner.recording && !owner.speaking);
		CHECK((owner.calls ==
		       std::vector<std::string>{"prepare", "cancel_output", "microphone_start"}));
		CHECK(!owner.accept_late_synthesis(prior_generation));
		CHECK(!intent.begin() && owner.calls.size() == 3);
		CHECK(intent.end() && !intent.active() && !owner.recording && owner.after == 1);
		CHECK((owner.native_stop_confirmed == std::vector<bool>{true}));
		CHECK(
		    (owner.calls == std::vector<std::string>{"prepare", "cancel_output", "microphone_start",
		                                             "microphone_stop", "ending"}));
		CHECK(intent.end() && owner.after == 1 && owner.calls.size() == 5);
		CHECK(intent.begin() && owner.recording);
		CHECK(intent.end() && owner.after == 2); // A later explicit gesture is a new lifetime.
	}
	for (auto stage : {failure::prepare, failure::before, failure::start}) {
		for (bool throws : {false, true}) {
			fake_owner owner;
			if (throws)
				owner.throwing = stage;
			else
				owner.reject = stage;
			capture_intent intent(owner.callbacks());
			auto           started = intent.begin();
			CHECK(!started && !intent.active() && !owner.recording && !owner.pcm);
			CHECK(owner.after == (stage == failure::prepare ? 0 : 1));
			CHECK((owner.native_stop_confirmed ==
			       (stage == failure::prepare ? std::vector<bool>{} : std::vector<bool>{true})));
			if (stage == failure::prepare)
				CHECK((owner.calls == std::vector<std::string>{"prepare", "microphone_stop"}));
			else if (stage == failure::before)
				CHECK((owner.calls == std::vector<std::string>{"prepare", "cancel_output",
				                                               "microphone_stop", "ending"}));
			else
				CHECK((owner.calls == std::vector<std::string>{"prepare", "cancel_output",
				                                               "microphone_start",
				                                               "microphone_stop", "ending"}));
			const auto count = owner.calls.size();
			CHECK(intent.end() && owner.calls.size() == count);
		}
	}
	{
		fake_owner owner;
		owner.start_closes_then_fails = true;
		owner.reject                  = failure::stop;
		capture_intent intent(owner.callbacks());
		auto           started = intent.begin();
		CHECK(!started && !intent.active() && owner.after == 1);
		CHECK((owner.native_stop_confirmed == std::vector<bool>{false}));
		CHECK((owner.calls == std::vector<std::string>{"prepare", "cancel_output",
		                                               "microphone_start", "microphone_stop",
		                                               "microphone_stop", "ending"}));
		owner.reject = failure::none;
		auto retried = owner.stop_native();
		CHECK(!retried && retried.error().native_code == 42);
		CHECK(intent.end() && owner.after == 1);
	}
	for (auto stage : {failure::stop, failure::ending}) {
		fake_owner     owner;
		capture_intent intent(owner.callbacks());
		CHECK(intent.begin());
		owner.throwing = stage;
		auto ended     = intent.end();
		CHECK(!ended && !ended.error().message.empty() && owner.after == 1);
		CHECK((owner.native_stop_confirmed == std::vector<bool>{stage != failure::stop}));
		CHECK(!owner.recording && !intent.active() && owner.calls.back() == "ending");
		CHECK(intent.end() && owner.after == 1);
	}
	{
		fake_owner     owner;
		capture_intent intent(owner.callbacks());
		CHECK(intent.begin());
		owner.reject = failure::stop;
		auto ended   = intent.end();
		CHECK(!ended && ended.error().native_code == 42 && owner.after == 1);
		CHECK((owner.native_stop_confirmed == std::vector<bool>{false}));
		CHECK(intent.end() && owner.after == 1);
	}
	{
		fake_owner owner;
		owner.reject   = failure::before;
		owner.throwing = failure::stop;
		capture_intent intent(owner.callbacks());
		auto           started = intent.begin();
		CHECK(!started &&
		      started.error().message.find("cancel_output rejected") != std::string::npos);
		CHECK(started.error().message.find("cleanup: Capture stop callback threw") !=
		      std::string::npos);
		CHECK(started.error().native_code == 42 && owner.after == 1);
		CHECK((owner.native_stop_confirmed == std::vector<bool>{false}));
	}
	{
		fake_owner     owner;
		capture_intent intent(owner.callbacks());
		CHECK(intent.begin());
		owner.reject   = failure::stop;
		owner.throwing = failure::ending;
		auto ended     = intent.end();
		CHECK(!ended && ended.error().native_code == 42 && owner.after == 1);
		CHECK((owner.native_stop_confirmed == std::vector<bool>{false}));
		CHECK(ended.error().message.find("microphone_stop rejected") != std::string::npos);
		CHECK(ended.error().message.find("ending notification threw") != std::string::npos);
		CHECK(intent.end() && owner.after == 1);
	}
	{
		fake_owner owner;
		{
			capture_intent intent(owner.callbacks());
			CHECK(intent.begin());
		} // Teardown must stop native input before balancing the client, without a frame poll.
		CHECK(!owner.recording && owner.after == 1 && owner.calls.back() == "ending");
		CHECK((owner.native_stop_confirmed == std::vector<bool>{true}));
	}
	{
		fake_owner     owner;
		capture_intent intent(owner.callbacks());
		CHECK(intent.end() && owner.calls.empty()); // No gesture/permission means no notification.
	}
	std::puts(
	    "Capture intent passed: prepared PCM, cancel-before-start, balanced failures and teardown");
}
