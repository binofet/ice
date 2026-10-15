#pragma once
#include "Core/GXDevice/icGLext.h"
#include <string>
// Context-owned linear HDR color + depth attachment, resize is transactional.
class icRenderTargetGL {
  public:
    icRenderTargetGL() = default;
    icRenderTargetGL(const icRenderTargetGL &) = delete;
    icRenderTargetGL &operator=(const icRenderTargetGL &) = delete;
    ~icRenderTargetGL() { Release(); }
    bool Resize(int width, int height, std::string &diagnostics);
    void Bind() const;
    static void BindFramebuffer(GLuint);
    static GLuint CurrentFramebuffer();
    GLuint Color() const { return color; }
    void Release();

  private:
    GLuint framebuffer = 0, color = 0, depth = 0;
    int width = 0, height = 0;
};
