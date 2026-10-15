#pragma once

#include "ice/render/gl/api.h"
#include <string>
namespace ice::render::gl {
/// Context-owned linear HDR color + depth attachment, resize is transactional.
class render_target {
public:
	render_target()                                 = default;
	render_target(const render_target &)            = delete;
	render_target &operator=(const render_target &) = delete;
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~render_target() { release(); }
	/// Resize owned render attachments transactionally; false records diagnostics and keeps the
	/// previous target.
	bool resize(int width, int height, std::string &diagnostics);
	/// Bind this resource in its live current context.
	void bind() const;
	/// Bind the supplied framebuffer name in the current context; ownership is unchanged.
	static void bind_framebuffer(GLuint framebuffer_id);
	/// Return the framebuffer currently bound in the active context.
	static GLuint current_framebuffer();
	/// Return the borrowed color-attachment texture name; recreation/release invalidates it.
	GLuint color() const { return color_; }
	/// Release owned native resources in their live owning context; do not retain previous native
	/// names.
	void release();

private:
	GLuint framebuffer_ = 0, color_ = 0, depth_ = 0;
	int    width_ = 0, height_ = 0;
};

} // namespace ice::render::gl
