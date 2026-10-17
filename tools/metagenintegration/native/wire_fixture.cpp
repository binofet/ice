#include "schema.generated.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <span>
#include <string>

namespace {
bool run(const char *path, bool emit, bool changed) {
	ice::generated::paddle_settings expected;
	if (changed) {
		expected.maximum_speed    = 6.25f;
		expected.simulation_seed  = -54;
		expected.boost            = 5.0f;
		expected.player_name      = "Snowman ☃";
		expected.predict_rebounds = false;
		expected.winning_score    = 1024;
	}
	auto bytes = ice::meta::serialize(ice::generated::paddle_settings_schema(),
	                                  ice::generated::to_record(expected));
	if (!bytes)
		return false;
	if (emit) {
		std::ofstream output(path, std::ios::binary);
		output.write(reinterpret_cast<const char *>(bytes->data()),
		             static_cast<std::streamsize>(bytes->size()));
		return bool(output);
	}
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return false;
	std::string                contents((std::istreambuf_iterator<char>(input)), {});
	std::span<const std::byte> actual(reinterpret_cast<const std::byte *>(contents.data()),
	                                  contents.size());
	auto decoded = ice::meta::deserialize(ice::generated::paddle_settings_schema(), actual);
	return decoded && *decoded == ice::generated::to_record(expected) &&
	       std::equal(actual.begin(), actual.end(), bytes->begin(), bytes->end());
}
} // namespace

int main(int count, char **args) {
	if (count != 4 || (std::string(args[1]) != "--emit" && std::string(args[1]) != "--verify"))
		return 2;
	bool emit = std::string(args[1]) == "--emit";
	if (!run(args[2], emit, false) || !run(args[3], emit, true))
		return 1;
	std::puts(emit ? "C++ fixtures emitted"
	               : "C++ accepted Rust bytes with exact field/value agreement");
	return 0;
}
