#include "../debug_trace.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "XR trace failed at line %d: %s\n", __LINE__, #condition);        \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static std::string read(const std::filesystem::path &path) {
	std::ifstream stream(path);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main() {
	const auto directory =
	    std::filesystem::temp_directory_path() /
	    ("ice-xr-trace-test-" +
	     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directory(directory);
	const auto current  = directory / "ice-xr-trace.log";
	const auto previous = directory / "ice-xr-trace.log.previous";
	{
		ice::examples::xr::debug_trace trace;
		CHECK(trace.initialize(directory.string()));
		CHECK(trace.write('I', "Build marker=space-anchor-v2 cplusplus=202302 pid=test"));
	}
	{
		ice::examples::xr::debug_trace trace;
		CHECK(trace.initialize(directory.string()));
		CHECK(read(current).find("space-anchor-v2") != std::string::npos);
		CHECK(trace.write('I', "Reopened after transport loss"));
		CHECK(read(current).find("Reopened after transport loss") != std::string::npos);
		const std::string long_record(1023, 'x');
		for (int index = 0; index < 200; ++index)
			CHECK(trace.write('I', long_record.c_str()));
		CHECK(trace.write('I', "Last retained frame=1200"));
		CHECK(std::filesystem::file_size(current) <= 32 * 1024);
		CHECK(std::filesystem::file_size(previous) <= 32 * 1024);
		CHECK(read(current).find("Last retained frame=1200") != std::string::npos);
		CHECK(!read(previous).empty());
	}
	std::filesystem::remove_all(directory);
	std::puts("XR trace passed: persistent reopen, flushed records and bounded 64 KiB rotation");
}
