#pragma once
#include <string>
#include "Core/GXDevice/icGLext.h"

// A context-owned GLSL program. Release before destroying its GL context.
// Compile is transactional: a failed replacement leaves the old program usable.
class icShaderGL {
public:
    icShaderGL() = default;
    ~icShaderGL() { Release(); }
    icShaderGL(const icShaderGL&) = delete;
    icShaderGL& operator=(const icShaderGL&) = delete;
    bool Compile(const char* vertex, const char* fragment, std::string& diagnostics);
    void Release();
    bool Valid() const { return program != 0; }
    GLuint Name() const { return program; }
    void Bind() const;
    static void Unbind();
    GLint Uniform(const char* name) const;
private:
    GLuint program = 0;
};
