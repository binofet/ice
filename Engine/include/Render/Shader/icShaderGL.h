#pragma once
#include "Core/GXDevice/icGLext.h"
#include "Render/Shader/icShader.h"
// Context-owned compiled GLSL stage. Release before destroying the context.
class icShaderGL final : public icShader {
  public:
    ~icShaderGL() { Release(); }
    icShaderGL() = default;
    icShaderGL(const icShaderGL &) = delete;
    icShaderGL &operator=(const icShaderGL &) = delete;
    bool Compile(const icShaderSource &, std::string &) override;
    bool Valid() const override { return shader != 0; }
    const icShaderSource &Source() const override { return source; }
    GLuint Name() const { return shader; }
    void Release();
    static bool Available();

  private:
    GLuint shader = 0;
    icShaderSource source;
};
