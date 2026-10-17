#pragma once
#include "ice/meta/schema.h"
#include <rttr/type>

namespace ice::examples::rttr_bridge {
/// Read generated RTTR metadata without constructing instances or invoking property accessors.
/// The scalar profile requires exact C++ property types; unsupported/unmarked registrations fail.
ice::expected<meta::schema_descriptor, meta::diagnostic> describe(rttr::type type);
} // namespace ice::examples::rttr_bridge
