#pragma once
#include <string>
#include <vector>

enum class icShaderStage { Vertex, Fragment };
enum class icShaderLanguage { GLSL, HLSL, MSL, SPIRV };
struct icShaderSource {
    icShaderStage stage = icShaderStage::Vertex;
    icShaderLanguage language = icShaderLanguage::GLSL;
    std::string source, entryPoint = "main", label, uri;
};
enum class icShaderBindingType { Value, Texture };
struct icShaderBinding {
    std::string name;
    icShaderBindingType type = icShaderBindingType::Value;
    unsigned elements = 1;
};
// One compiled stage. Source language/backend support is explicit; no GL handles or locations.
class icShader {
  public:
    virtual ~icShader() {}
    virtual bool Compile(const icShaderSource &, std::string &diagnostics) = 0;
    virtual bool Valid() const = 0;
    virtual const icShaderSource &Source() const = 0;
};
