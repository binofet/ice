#include "ice/render/gl/shader.h"
#include <cstdint>
#include <cstdlib>
#include <vector>
namespace ice::render::gl {
bool shader::available() {
#ifdef WIN32
#define LOAD(name, type)                                                                           \
	name = reinterpret_cast<type>(wglGetProcAddress(#name));                                       \
	if (!name || reinterpret_cast<uintptr_t>(name) <= 3 || reinterpret_cast<intptr_t>(name) == -1) \
	return false
	LOAD(glCreateShader, PFNGLCREATESHADER);
	LOAD(glShaderSource, PFNGLSHADERSOURCE);
	LOAD(glCompileShader, PFNGLCOMPILESHADER);
	LOAD(glGetShaderiv, PFNGLGETSHADERIV);
	LOAD(glGetShaderInfoLog, PFNGLGETSHADERINFOLOG);
	LOAD(glDeleteShader, PFNGLDELETESHADER);
	LOAD(glCreateProgram, PFNGLCREATEPROGRAM);
	LOAD(glAttachShader, PFNGLATTACHSHADER);
	LOAD(glLinkProgram, PFNGLLINKPROGRAM);
	LOAD(glGetProgramiv, PFNGLGETPROGRAMIV);
	LOAD(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOG);
	LOAD(glDeleteProgram, PFNGLDELETEPROGRAM);
	LOAD(glGetActiveUniform, PFNGLGETACTIVEUNIFORM);
	LOAD(glUseProgram, PFNGLUSEPROGRAM);
	LOAD(glGetUniformLocation, PFNGLGETUNIFORMLOCATION);
	LOAD(glActiveTexture, PFNGLACTIVETEXTURE);
	LOAD(glClientActiveTexture, PFNGLCLIENTACTIVETEXTURE);
	LOAD(glUniform1i, PFNGLUNIFORM1I);
	LOAD(glUniform1f, PFNGLUNIFORM1F);
	LOAD(glUniform2f, PFNGLUNIFORM2F);
	LOAD(glUniform3f, PFNGLUNIFORM3F);
	LOAD(glUniform4f, PFNGLUNIFORM4F);
	LOAD(glUniformMatrix4fv, PFNGLUNIFORMMATRIX4FV);
#undef LOAD
#endif
	const auto version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
	return version && std::atoi(version) >= 2;
}
bool shader::compile(const ice::render::shader_source &source, std::string &diagnostics) {
	diagnostics.clear();
	if (source.language != ice::render::shader_language::glsl || source.entry_point != "main") {
		diagnostics = "GL compatibility backend requires GLSL with entry point main";
		return false;
	}
	if (!available()) {
		diagnostics = "OpenGL 2.0 / GLSL 1.20 unavailable";
		return false;
	}
	GLuint next = glCreateShader(
	    source.stage == ice::render::shader_stage::vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
	if (!next) {
		diagnostics = "Cannot allocate shader stage";
		return false;
	}
	const char *bytes = source.source.c_str();
	glShaderSource(next, 1, &bytes, nullptr);
	glCompileShader(next);
	GLint ok = 0, size = 0;
	glGetShaderiv(next, GL_COMPILE_STATUS, &ok);
	glGetShaderiv(next, GL_INFO_LOG_LENGTH, &size);
	if (size > 1) {
		std::vector<char> text(size);
		glGetShaderInfoLog(next, size, nullptr, text.data());
		diagnostics = (source.uri.empty() ? source.label : source.uri) + ": " + text.data();
	}
	if (!ok) {
		if (diagnostics.empty())
			diagnostics = "Shader compilation failed";
		glDeleteShader(next);
		return false;
	}
	release();
	shader_ = next;
	source_ = source;
	return true;
}
void shader::release() {
	if (shader_) {
		glDeleteShader(shader_);
		shader_ = 0;
	}
}

} // namespace ice::render::gl
