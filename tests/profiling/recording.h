#pragma once
#include "ice/profiling/profile.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace ice::profiling::test {
enum class event_kind { begin, end, text, value, metadata, frame, plot };
struct event {
	event_kind    kind;
	const char   *name = nullptr;
	std::uint64_t id = 0, value = 0;
	char          text[max_annotation_bytes + 1]{};
	std::size_t   text_size = 0;
};
struct recording_stats {
	unsigned starts = 0, stops = 0, dropped = 0;
};
std::span<const event> events() noexcept;
recording_stats        stats() noexcept;
void                   set_connected(bool connected) noexcept;
} // namespace ice::profiling::test
