#pragma once
#include "ice/core/expected.h"
#include "ice/platform/desktop_window.h"
#include <memory>
#include <string>
namespace ice::render::gl::detail {
using raw_proc = void (*)();
// Native context ownership stays outside the graphics command implementation. The root
// context supports preparation/readback; each presentation context shares its resources.
class native_context {
public:
	virtual ~native_context()                      = default;
	virtual bool     make_current()                = 0;
	virtual void     resize(int width, int height) = 0;
	virtual bool     present()                     = 0;
	virtual raw_proc resolve(const char *name)     = 0;
};
ice::expected<std::shared_ptr<native_context>, std::string>
create_context(std::shared_ptr<native_context>            shared = {},
               const ice::platform::native_window_handle *target = nullptr);
} // namespace ice::render::gl::detail
