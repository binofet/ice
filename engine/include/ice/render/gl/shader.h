#pragma once

#include "ice/render/gl/api.h"
#include "ice/render/shader.h"
namespace ice::render::gl {
/// Context-owned compiled GLSL stage. Release before destroying the context.
class shader final : public ice::render::shader {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~shader() { release(); }
	shader()                          = default;
	shader(const shader &)            = delete;
	shader &operator=(const shader &) = delete;
	/// Compile a stage transactionally; failure returns diagnostics and retains the last valid
	/// stage.
	bool compile(const ice::render::shader_source &source, std::string &diagnostics) override;
	/// Return whether this object currently holds a successfully created native resource.
	bool valid() const override { return shader_ != 0; }
	/// Borrow the successfully compiled source description; it remains owned by the stage.
	const ice::render::shader_source &source() const override { return source_; }
	/// Return a borrowed native resource name; callers must not delete it independently.
	GLuint name() const { return shader_; }
	/// Release owned native resources in their live owning context; do not retain previous native
	/// names.
	void release();
	/// Test the current context for the backend capabilities required by this implementation.
	static bool available();

private:
	GLuint                     shader_ = 0;
	ice::render::shader_source source_;
};

} // namespace ice::render::gl
