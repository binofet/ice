#include "Render/Texture/icRenderTargetGL.h"
#include <cstdint>
#if defined(__APPLE__)
#include <dlfcn.h>
#elif !defined(WIN32)
#include <GL/glx.h>
#endif
namespace {
using Gen = void(APIENTRY *)(GLsizei, GLuint *);
using Del = void(APIENTRY *)(GLsizei, const GLuint *);
using Bind = void(APIENTRY *)(GLenum, GLuint);
using Check = GLenum(APIENTRY *)(GLenum);
using AttachTexture = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
using Store = void(APIENTRY *)(GLenum, GLenum, GLsizei, GLsizei);
using AttachDepth = void(APIENTRY *)(GLenum, GLenum, GLenum, GLuint);
Gen genFBO = nullptr, genRBO = nullptr;
Del delFBO = nullptr, delRBO = nullptr;
Bind bindFBO = nullptr, bindRBO = nullptr;
Check checkFBO = nullptr;
AttachTexture attachTexture = nullptr;
Store storage = nullptr;
AttachDepth attachDepth = nullptr;
void *proc(const char *name) {
#ifdef WIN32
    auto result = wglGetProcAddress(name);
    auto value = reinterpret_cast<std::uintptr_t>(result);
    return value <= 3 || value == std::uintptr_t(-1) ? nullptr : reinterpret_cast<void *>(result);
#elif defined(__APPLE__)
    return dlsym(RTLD_DEFAULT, name);
#else
    return reinterpret_cast<void *>(glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name)));
#endif
}
bool load() {
#define GET(target, type, name)                                                                    \
    target = reinterpret_cast<type>(proc(name));                                                   \
    if (!target)                                                                                   \
    return false
    GET(genFBO, Gen, "glGenFramebuffersEXT");
    GET(genRBO, Gen, "glGenRenderbuffersEXT");
    GET(delFBO, Del, "glDeleteFramebuffersEXT");
    GET(delRBO, Del, "glDeleteRenderbuffersEXT");
    GET(bindFBO, Bind, "glBindFramebufferEXT");
    GET(bindRBO, Bind, "glBindRenderbufferEXT");
    GET(checkFBO, Check, "glCheckFramebufferStatusEXT");
    GET(attachTexture, AttachTexture, "glFramebufferTexture2DEXT");
    GET(storage, Store, "glRenderbufferStorageEXT");
    GET(attachDepth, AttachDepth, "glFramebufferRenderbufferEXT");
#undef GET
    return true;
}
const GLenum Framebuffer = 0x8D40, Renderbuffer = 0x8D41, ColorAttachment = 0x8CE0,
             DepthAttachment = 0x8D00, Complete = 0x8CD5, Depth24 = 0x81A6, RGBA16F = 0x881A;
} // namespace
GLuint icRenderTargetGL::CurrentFramebuffer() {
    GLint value = 0;
    glGetIntegerv(0x8CA6, &value);
    return (GLuint)value;
}
void icRenderTargetGL::BindFramebuffer(GLuint target) { bindFBO(Framebuffer, target); }
void icRenderTargetGL::Bind() const { bindFBO(Framebuffer, framebuffer); }
void icRenderTargetGL::Release() {
    if (depth)
        delRBO(1, &depth);
    if (color)
        glDeleteTextures(1, &color);
    if (framebuffer)
        delFBO(1, &framebuffer);
    depth = color = framebuffer = 0;
    width = height = 0;
}
bool icRenderTargetGL::Resize(int w, int h, std::string &diagnostics) {
    if (w == width && h == height && framebuffer)
        return true;
    if (!load()) {
        diagnostics = "GL_EXT_framebuffer_object unavailable";
        return false;
    }
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (w <= 0 || h <= 0 || w > maximum || h > maximum) {
        diagnostics = "Render target dimensions outside GL limits";
        return false;
    }
    GLuint old = CurrentFramebuffer();
    GLint oldTexture = 0, oldRBO = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glGetIntegerv(0x8CA7, &oldRBO);
    icRenderTargetGL next;
    next.width = w;
    next.height = h;
    genFBO(1, &next.framebuffer);
    genRBO(1, &next.depth);
    glGenTextures(1, &next.color);
    next.Bind();
    glBindTexture(GL_TEXTURE_2D, next.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    attachTexture(Framebuffer, ColorAttachment, GL_TEXTURE_2D, next.color, 0);
    bindRBO(Renderbuffer, next.depth);
    storage(Renderbuffer, Depth24, w, h);
    attachDepth(Framebuffer, DepthAttachment, Renderbuffer, next.depth);
    bool ok = checkFBO(Framebuffer) == Complete && glGetError() == GL_NO_ERROR;
    BindFramebuffer(old);
    bindRBO(Renderbuffer, oldRBO);
    glBindTexture(GL_TEXTURE_2D, oldTexture);
    if (!ok) {
        diagnostics = "Cannot create linear RGBA16F/depth24 render target";
        return false;
    }
    Release();
    framebuffer = next.framebuffer;
    color = next.color;
    depth = next.depth;
    width = w;
    height = h;
    next.framebuffer = next.color = next.depth = 0;
    return true;
}
