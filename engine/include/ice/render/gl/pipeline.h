#pragma once

#include "ice/render/gl/shader.h"
#include "ice/render/pipeline.h"
namespace ice::render::gl {
class pipeline final : public ice::render::pipeline {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~pipeline() { release(); }
	pipeline()                            = default;
	pipeline(const pipeline &)            = delete;
	pipeline &operator=(const pipeline &) = delete;
	/// Create/replace owned resources from the supplied description; inspect status before using
	/// the new resource.
	bool create(const ice::render::pipeline_desc &description, std::string &diagnostics) override;
	/// Return whether this object currently holds a successfully created native resource.
	bool valid() const override { return program_ != 0; }
	/// Borrow the successfully created pipeline description and retained shader stages.
	const ice::render::pipeline_desc &descriptor() const override { return desc_; }
	/// Borrow the reflected linked-program bindings; recreation invalidates prior references.
	const std::vector<ice::render::shader_binding> &bindings() const override { return bindings_; }
	/// Apply this pipelines GL state and program in its live owning context.
	void apply() const;
	/// Return the native uniform location, or -1 when the linked program has no such active
	/// uniform.
	GLint uniform(const char *name) const;
	/// Return a borrowed native resource name; callers must not delete it independently.
	GLuint name() const { return program_; }
	/// Release owned native resources in their live owning context; do not retain previous native
	/// names.
	void release();

private:
	GLuint                                   program_ = 0;
	ice::render::pipeline_desc               desc_;
	std::vector<ice::render::shader_binding> bindings_;
};
/// Transitional linked-program wrapper for compatibility samples, now correctly named.
class program {
public:
	/// Compile a stage transactionally; failure returns diagnostics and retains the last valid
	/// stage.
	bool compile(const char *vertex, const char *fragment, std::string &diagnostics);
	/// Release owned native resources in their live owning context; do not retain previous native
	/// names.
	void release() { pipeline_.release(); }
	/// Return whether this object currently holds a successfully created native resource.
	bool valid() const { return pipeline_.valid(); }
	/// Return a borrowed native resource name; callers must not delete it independently.
	GLuint name() const { return pipeline_.name(); }
	/// Bind this resource in its live current context.
	void bind() const { glUseProgram(pipeline_.name()); }
	/// Bind the default/no-program state in the current context.
	static void unbind() { glUseProgram(0); }
	/// Return the native uniform location, or -1 when the linked program has no such active
	/// uniform.
	GLint uniform(const char *name) const { return pipeline_.uniform(name); }

private:
	pipeline pipeline_;
};

} // namespace ice::render::gl
