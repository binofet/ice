#include "ice/audio/playback.h"
#include <cstdio>
#include <cstdlib>
#include <string_view>

int main() {
	// This process must select only the upstream null backend before the first
	// OpenAL call. Never probe physical output when invoked without this guard.
	const auto *drivers = std::getenv("ALSOFT_DRIVERS");
	if (!drivers || std::string_view(drivers) != "null") {
		std::fputs("Native rejection test requires exact ALSOFT_DRIVERS=null; no output opened\n",
		           stderr);
		return 1;
	}
	ice::audio::playback_options options;
	options.mode = ice::audio::playback_mode::native;
	options.hrtf = ice::audio::hrtf_policy::disabled;
	auto device  = ice::audio::playback_device::create(options);
	if (device || device.error().message.find("nonphysical backend") == std::string::npos ||
	    device.error().message.find("No Output") == std::string::npos) {
		std::fprintf(stderr, "Expected actual null-backend rejection, observed: %s\n",
		             device ? "native null device incorrectly accepted"
		                    : device.error().message.c_str());
		return 1;
	}
	std::printf("Native rejection passed with null-only backend: %s\n",
	            device.error().message.c_str());
}
