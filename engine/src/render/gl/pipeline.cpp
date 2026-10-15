#include "ice/render/gl/pipeline.h"
#include <set>
#include <vector>
namespace ice::render::gl {
bool pipeline::create(const ice::render::pipeline_desc &description, std::string &diagnostics) {
	diagnostics.clear();
	auto v = dynamic_cast<shader *>(description.vertex.get());
	auto f = dynamic_cast<shader *>(description.fragment.get());
	if (!v || !f || !v->valid() || !f->valid() ||
	    v->source().stage != ice::render::shader_stage::vertex ||
	    f->source().stage != ice::render::shader_stage::fragment) {
		diagnostics = "GL pipeline requires valid vertex and fragment GL stages";
		return false;
	}
	std::set<ice::render::vertex_semantic> seen;
	for (auto &a : description.vertices.attributes) {
		unsigned size = (unsigned)a.format * 4;
		if ((unsigned)a.format < 2 || (unsigned)a.format > 4 ||
		    a.offset > description.vertices.stride ||
		    size > description.vertices.stride - a.offset || !seen.insert(a.semantic).second ||
		    (a.semantic == ice::render::vertex_semantic::normal &&
		     a.format != ice::render::vertex_format::float3)) {
			diagnostics = "Invalid vertex layout declaration";
			return false;
		}
	}
	GLuint next = glCreateProgram();
	if (!next) {
		diagnostics = "Cannot allocate GL pipeline";
		return false;
	}
	glAttachShader(next, v->name());
	glAttachShader(next, f->name());
	glLinkProgram(next);
	GLint ok = 0, size = 0;
	glGetProgramiv(next, GL_LINK_STATUS, &ok);
	glGetProgramiv(next, GL_INFO_LOG_LENGTH, &size);
	if (size > 1) {
		std::vector<char> text(size);
		glGetProgramInfoLog(next, size, nullptr, text.data());
		diagnostics = description.label + ": " + text.data();
	}
	if (!ok) {
		if (diagnostics.empty())
			diagnostics = "GL pipeline link failed";
		glDeleteProgram(next);
		return false;
	}
	std::vector<ice::render::shader_binding> reflected;
	GLint                                    count = 0, length = 0;
	glGetProgramiv(next, GL_ACTIVE_UNIFORMS, &count);
	glGetProgramiv(next, GL_ACTIVE_UNIFORM_MAX_LENGTH, &length);
	std::vector<char> name(length > 0 ? length : 1);
	for (GLint i = 0; i < count; i++) {
		GLsizei actual   = 0;
		GLint   elements = 0;
		GLenum  type     = 0;
		glGetActiveUniform(next, i, (GLsizei)name.size(), &actual, &elements, &type, name.data());
		ice::render::shader_binding b;
		b.name.assign(name.data(), actual);
		b.elements = elements;
		b.type     = type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE
		                 ? ice::render::shader_binding_type::texture
		                 : ice::render::shader_binding_type::value;
		reflected.push_back(b);
	}
	release();
	program_  = next;
	desc_     = description;
	bindings_ = std::move(reflected);
	return true;
}
void pipeline::release() {
	if (program_) {
		glDeleteProgram(program_);
		program_ = 0;
	}
	desc_ = ice::render::pipeline_desc();
	bindings_.clear();
}
GLint pipeline::uniform(const char *name) const {
	return program_ ? glGetUniformLocation(program_, name) : -1;
}
void pipeline::apply() const {
	glUseProgram(program_);
	auto &s = desc_.state;
	if (s.depth_test)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(s.depth_write ? GL_TRUE : GL_FALSE);
	if (s.cull == ice::render::cull_mode::none)
		glDisable(GL_CULL_FACE);
	else {
		glEnable(GL_CULL_FACE);
		glCullFace(s.cull == ice::render::cull_mode::back ? GL_BACK : GL_FRONT);
	}
	glFrontFace(s.clockwise_front ? GL_CW : GL_CCW);
	if (s.blend == ice::render::blend_mode::alpha) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else
		glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, s.wireframe ? GL_LINE : GL_FILL);
}
bool program::compile(const char *vertex, const char *fragment, std::string &diagnostics) {
	auto                       v = std::make_shared<shader>(), f = std::make_shared<shader>();
	ice::render::shader_source source;
	source.source = vertex ? vertex : "";
	source.stage  = ice::render::shader_stage::vertex;
	if (!v->compile(source, diagnostics))
		return false;
	source.source = fragment ? fragment : "";
	source.stage  = ice::render::shader_stage::fragment;
	if (!f->compile(source, diagnostics))
		return false;
	ice::render::pipeline_desc desc;
	desc.vertex   = v;
	desc.fragment = f;
	return pipeline_.create(desc, diagnostics);
}

} // namespace ice::render::gl
