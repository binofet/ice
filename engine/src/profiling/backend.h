#pragma once
#include "ice/profiling/profile.h"
namespace ice::profiling::backend {
void         start() noexcept;
void         stop() noexcept;
bool         connected() noexcept;
zone_context begin(const source_location *source) noexcept;
void         end(zone_context context) noexcept;
void         text(zone_context context, std::string_view text) noexcept;
void         value(zone_context context, std::uint64_t value) noexcept;
void         frame() noexcept;
void         plot(const char *name, std::int64_t value) noexcept;
void         metadata(std::string_view value) noexcept;
} // namespace ice::profiling::backend
