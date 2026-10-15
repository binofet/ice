#include "Render/Shader/icShaderGL.h"
#include <cstdint>
#include <cstdlib>
#include <vector>
bool icShaderGL::Available() {
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
bool icShaderGL::Compile(const icShaderSource &candidate, std::string &diagnostics) {
    diagnostics.clear();
    if (candidate.language != icShaderLanguage::GLSL || candidate.entryPoint != "main") {
        diagnostics = "GL compatibility backend requires GLSL with entry point main";
        return false;
    }
    if (!Available()) {
        diagnostics = "OpenGL 2.0 / GLSL 1.20 unavailable";
        return false;
    }
    GLuint next = glCreateShader(candidate.stage == icShaderStage::Vertex ? GL_VERTEX_SHADER
                                                                          : GL_FRAGMENT_SHADER);
    if (!next) {
        diagnostics = "Cannot allocate shader stage";
        return false;
    }
    const char *bytes = candidate.source.c_str();
    glShaderSource(next, 1, &bytes, nullptr);
    glCompileShader(next);
    GLint ok = 0, size = 0;
    glGetShaderiv(next, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(next, GL_INFO_LOG_LENGTH, &size);
    if (size > 1) {
        std::vector<char> text(size);
        glGetShaderInfoLog(next, size, nullptr, text.data());
        diagnostics =
            (candidate.uri.empty() ? candidate.label : candidate.uri) + ": " + text.data();
    }
    if (!ok) {
        if (diagnostics.empty())
            diagnostics = "Shader compilation failed";
        glDeleteShader(next);
        return false;
    }
    Release();
    shader = next;
    source = candidate;
    return true;
}
void icShaderGL::Release() {
    if (shader) {
        glDeleteShader(shader);
        shader = 0;
    }
}
