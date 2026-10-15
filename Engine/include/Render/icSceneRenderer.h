#pragma once
#include "Content/icSceneAsset.h"
#include <memory>
struct icSceneView {
    std::array<float, 16> view, projection;
    int width = 1, height = 1;
    int debugMode = 0; // 0 lit, 1 base color, 2 normals, 3 metallic/roughness, 4 UV
    bool wireframe = false;
    int material = -1; // -1 all materials; otherwise isolate one
    float exposure = 1;
};
class icSceneRenderer {
  public:
    virtual ~icSceneRenderer() {}
    virtual bool Upload(const icSceneAsset &, std::string &diagnostics) = 0;
    virtual bool Draw(const icSceneView &, std::string &diagnostics) = 0;
    virtual void Release() = 0;
};
// Explicit backend selection; callers use the same asset/view interface.
std::unique_ptr<icSceneRenderer>
icCreateSceneRendererGL(std::shared_ptr<icResourceResolver> resolver);
