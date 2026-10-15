#pragma once
#include "Render/Shader/icShader.h"
#include <memory>

enum class icCullMode { None, Back, Front };
enum class icBlendMode { Opaque, Alpha };
struct icPipelineState {
    bool depthTest = true, depthWrite = true, clockwiseFront = false, wireframe = false;
    icCullMode cull = icCullMode::Back;
    icBlendMode blend = icBlendMode::Opaque;
};
enum class icVertexSemantic { Position, Normal, Tangent, TexCoord0, TexCoord1, Color0 };
enum class icVertexFormat { Float2 = 2, Float3 = 3, Float4 = 4 };
struct icVertexAttribute {
    icVertexSemantic semantic;
    icVertexFormat format;
    unsigned offset;
    icVertexAttribute(icVertexSemantic s, icVertexFormat f, unsigned o)
        : semantic(s), format(f), offset(o) {}
};
struct icVertexLayout {
    unsigned stride = 0;
    std::vector<icVertexAttribute> attributes;
};
enum class icColorTargetFormat { DisplayRGBA8, LinearRGBA16Float };
struct icPipelineDesc {
    std::shared_ptr<icShader> vertex, fragment;
    icPipelineState state;
    icVertexLayout vertices;
    icColorTargetFormat colorTarget = icColorTargetFormat::DisplayRGBA8;
    std::string label;
};
// Creation retains stages. Command encoders/backends select pipelines; resources are not
// assigned through per-backend uniform locations in this common interface.
class icPipeline {
  public:
    virtual ~icPipeline() {}
    virtual bool Create(const icPipelineDesc &, std::string &diagnostics) = 0;
    virtual bool Valid() const = 0;
    virtual const icPipelineDesc &Descriptor() const = 0;
    virtual const std::vector<icShaderBinding> &Bindings() const = 0;
};
