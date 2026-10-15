#include "Render/Shader/icPipelineGL.h"
#include <set>
#include <vector>
bool icPipelineGL::Create(const icPipelineDesc &candidate, std::string &diagnostics) {
    diagnostics.clear();
    auto v = dynamic_cast<icShaderGL *>(candidate.vertex.get());
    auto f = dynamic_cast<icShaderGL *>(candidate.fragment.get());
    if (!v || !f || !v->Valid() || !f->Valid() || v->Source().stage != icShaderStage::Vertex ||
        f->Source().stage != icShaderStage::Fragment) {
        diagnostics = "GL pipeline requires valid vertex and fragment GL stages";
        return false;
    }
    std::set<icVertexSemantic> seen;
    for (auto &a : candidate.vertices.attributes) {
        unsigned size = (unsigned)a.format * 4;
        if ((unsigned)a.format < 2 || (unsigned)a.format > 4 ||
            a.offset > candidate.vertices.stride || size > candidate.vertices.stride - a.offset ||
            !seen.insert(a.semantic).second ||
            (a.semantic == icVertexSemantic::Normal && a.format != icVertexFormat::Float3)) {
            diagnostics = "Invalid vertex layout declaration";
            return false;
        }
    }
    GLuint next = glCreateProgram();
    if (!next) {
        diagnostics = "Cannot allocate GL pipeline";
        return false;
    }
    glAttachShader(next, v->Name());
    glAttachShader(next, f->Name());
    glLinkProgram(next);
    GLint ok = 0, size = 0;
    glGetProgramiv(next, GL_LINK_STATUS, &ok);
    glGetProgramiv(next, GL_INFO_LOG_LENGTH, &size);
    if (size > 1) {
        std::vector<char> text(size);
        glGetProgramInfoLog(next, size, nullptr, text.data());
        diagnostics = candidate.label + ": " + text.data();
    }
    if (!ok) {
        if (diagnostics.empty())
            diagnostics = "GL pipeline link failed";
        glDeleteProgram(next);
        return false;
    }
    std::vector<icShaderBinding> reflected;
    GLint count = 0, length = 0;
    glGetProgramiv(next, GL_ACTIVE_UNIFORMS, &count);
    glGetProgramiv(next, GL_ACTIVE_UNIFORM_MAX_LENGTH, &length);
    std::vector<char> name(length > 0 ? length : 1);
    for (GLint i = 0; i < count; i++) {
        GLsizei actual = 0;
        GLint elements = 0;
        GLenum type = 0;
        glGetActiveUniform(next, i, (GLsizei)name.size(), &actual, &elements, &type, name.data());
        icShaderBinding b;
        b.name.assign(name.data(), actual);
        b.elements = elements;
        b.type = type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE ? icShaderBindingType::Texture
                                                                  : icShaderBindingType::Value;
        reflected.push_back(b);
    }
    Release();
    program = next;
    desc = candidate;
    bindings = std::move(reflected);
    return true;
}
void icPipelineGL::Release() {
    if (program) {
        glDeleteProgram(program);
        program = 0;
    }
    desc = icPipelineDesc();
    bindings.clear();
}
GLint icPipelineGL::Uniform(const char *name) const {
    return program ? glGetUniformLocation(program, name) : -1;
}
void icPipelineGL::Apply() const {
    glUseProgram(program);
    auto &s = desc.state;
    if (s.depthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(s.depthWrite ? GL_TRUE : GL_FALSE);
    if (s.cull == icCullMode::None)
        glDisable(GL_CULL_FACE);
    else {
        glEnable(GL_CULL_FACE);
        glCullFace(s.cull == icCullMode::Back ? GL_BACK : GL_FRONT);
    }
    glFrontFace(s.clockwiseFront ? GL_CW : GL_CCW);
    if (s.blend == icBlendMode::Alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else
        glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, s.wireframe ? GL_LINE : GL_FILL);
}
bool icProgramGL::Compile(const char *vertex, const char *fragment, std::string &diagnostics) {
    auto v = std::make_shared<icShaderGL>(), f = std::make_shared<icShaderGL>();
    icShaderSource source;
    source.source = vertex ? vertex : "";
    source.stage = icShaderStage::Vertex;
    if (!v->Compile(source, diagnostics))
        return false;
    source.source = fragment ? fragment : "";
    source.stage = icShaderStage::Fragment;
    if (!f->Compile(source, diagnostics))
        return false;
    icPipelineDesc desc;
    desc.vertex = v;
    desc.fragment = f;
    return pipeline.Create(desc, diagnostics);
}
