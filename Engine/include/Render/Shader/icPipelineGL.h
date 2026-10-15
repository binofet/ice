#pragma once
#include "Render/Shader/icPipeline.h"
#include "Render/Shader/icShaderGL.h"
class icPipelineGL final : public icPipeline {
  public:
    ~icPipelineGL() { Release(); }
    icPipelineGL() = default;
    icPipelineGL(const icPipelineGL &) = delete;
    icPipelineGL &operator=(const icPipelineGL &) = delete;
    bool Create(const icPipelineDesc &, std::string &) override;
    bool Valid() const override { return program != 0; }
    const icPipelineDesc &Descriptor() const override { return desc; }
    const std::vector<icShaderBinding> &Bindings() const override { return bindings; }
    void Apply() const;
    GLint Uniform(const char *) const;
    GLuint Name() const { return program; }
    void Release();

  private:
    GLuint program = 0;
    icPipelineDesc desc;
    std::vector<icShaderBinding> bindings;
};
// Transitional linked-program wrapper for compatibility samples, now correctly named.
class icProgramGL {
  public:
    bool Compile(const char *vertex, const char *fragment, std::string &diagnostics);
    void Release() { pipeline.Release(); }
    bool Valid() const { return pipeline.Valid(); }
    GLuint Name() const { return pipeline.Name(); }
    void Bind() const { glUseProgram(pipeline.Name()); }
    static void Unbind() { glUseProgram(0); }
    GLint Uniform(const char *name) const { return pipeline.Uniform(name); }

  private:
    icPipelineGL pipeline;
};
