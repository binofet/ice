#include "ice/render/gl/render_target.h"
#include <cstdint>
namespace ice::render::gl {
#if defined(__APPLE__)
} // namespace ice::render::gl
#include <dlfcn.h>
namespace ice::render::gl {
#elif !defined(WIN32)
} // namespace ice::render::gl
#include <GL/glx.h>
namespace ice::render::gl {
#endif
namespace {
using gen                 = void(APIENTRY *)(GLsizei, GLuint *);
using del                 = void(APIENTRY *)(GLsizei, const GLuint *);
using bind                = void(APIENTRY *)(GLenum, GLuint);
using check               = GLenum(APIENTRY *)(GLenum);
using attach_texture_fn   = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
using store               = void(APIENTRY *)(GLenum, GLenum, GLsizei, GLsizei);
using attach_depth_fn     = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint);
gen               gen_fbo = nullptr, gen_rbo = nullptr;
del               del_fbo = nullptr, del_rbo = nullptr;
bind              bind_fbo = nullptr, bind_rbo = nullptr;
check             check_fbo      = nullptr;
attach_texture_fn attach_texture = nullptr;
store             storage        = nullptr;
attach_depth_fn   attach_depth   = nullptr;
void             *proc(const char *name) {
#ifdef WIN32
	auto result = wglGetProcAddress(name);
	auto value  = reinterpret_cast<std::uintptr_t>(result);
	return value <= 3 || value == std::uintptr_t(-1) ? nullptr : reinterpret_cast<void *>(result);
#elif defined(__APPLE__)
	return dlsym(RTLD_DEFAULT, name);
#else
	return reinterpret_cast<void *>(glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name)));
#endif
}
template <class Function> bool load_proc(Function &function, const char *name) {
	function = reinterpret_cast<Function>(proc(name));
	return function != nullptr;
}
bool load() {
	return load_proc(gen_fbo, "glGenFramebuffersEXT") &&
	       load_proc(gen_rbo, "glGenRenderbuffersEXT") &&
	       load_proc(del_fbo, "glDeleteFramebuffersEXT") &&
	       load_proc(del_rbo, "glDeleteRenderbuffersEXT") &&
	       load_proc(bind_fbo, "glBindFramebufferEXT") &&
	       load_proc(bind_rbo, "glBindRenderbufferEXT") &&
	       load_proc(check_fbo, "glCheckFramebufferStatusEXT") &&
	       load_proc(attach_texture, "glFramebufferTexture2DEXT") &&
	       load_proc(storage, "glRenderbufferStorageEXT") &&
	       load_proc(attach_depth, "glFramebufferRenderbufferEXT");
}
const GLenum framebuffer = 0x8D40, renderbuffer = 0x8D41, color_attachment = 0x8CE0,
             depth_attachment = 0x8D00, complete = 0x8CD5, depth24 = 0x81A6, rgba16_f = 0x881A;
} // namespace
GLuint render_target::current_framebuffer() {
	GLint value = 0;
	glGetIntegerv(0x8CA6, &value);
	return (GLuint)value;
}
void render_target::bind_framebuffer(GLuint framebuffer_id) {
	bind_fbo(framebuffer, framebuffer_id);
}
void render_target::bind() const { bind_fbo(framebuffer, framebuffer_); }
void render_target::release() {
	if (depth_)
		del_rbo(1, &depth_);
	if (color_)
		glDeleteTextures(1, &color_);
	if (framebuffer_)
		del_fbo(1, &framebuffer_);
	depth_ = color_ = framebuffer_ = 0;
	width_ = height_ = 0;
}
bool render_target::resize(int width, int height, std::string &diagnostics) {
	if (width == width_ && height == height_ && framebuffer_)
		return true;
	if (!load()) {
		diagnostics = "GL_EXT_framebuffer_object unavailable";
		return false;
	}
	GLint maximum = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
	if (width <= 0 || height <= 0 || width > maximum || height > maximum) {
		diagnostics = "Render target dimensions outside GL limits";
		return false;
	}
	GLuint old         = current_framebuffer();
	GLint  old_texture = 0, old_rbo = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture);
	glGetIntegerv(0x8CA7, &old_rbo);
	render_target next;
	next.width_  = width;
	next.height_ = height;
	gen_fbo(1, &next.framebuffer_);
	gen_rbo(1, &next.depth_);
	glGenTextures(1, &next.color_);
	next.bind();
	glBindTexture(GL_TEXTURE_2D, next.color_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, rgba16_f, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	attach_texture(framebuffer, color_attachment, GL_TEXTURE_2D, next.color_, 0);
	bind_rbo(renderbuffer, next.depth_);
	storage(renderbuffer, depth24, width, height);
	attach_depth(framebuffer, depth_attachment, renderbuffer, next.depth_);
	bool ok = check_fbo(framebuffer) == complete && glGetError() == GL_NO_ERROR;
	bind_framebuffer(old);
	bind_rbo(renderbuffer, old_rbo);
	glBindTexture(GL_TEXTURE_2D, old_texture);
	if (!ok) {
		diagnostics = "Cannot create linear RGBA16F/depth24 render target";
		return false;
	}
	release();
	framebuffer_      = next.framebuffer_;
	color_            = next.color_;
	depth_            = next.depth_;
	width_            = width;
	height_           = height;
	next.framebuffer_ = next.color_ = next.depth_ = 0;
	return true;
}

} // namespace ice::render::gl
