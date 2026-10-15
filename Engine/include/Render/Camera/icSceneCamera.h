#pragma once
#include "Render/icSceneRenderer.h"
// Right-handed, +Y up, OpenGL clip depth [-1,+1]. Unlike legacy GX SetViewMatrix,
// the returned view is already world-to-camera and must not be inverted again.
class icSceneCamera {
  public:
    void Fit(const icSceneAsset &);
    void Orbit(float x, float y);
    void Zoom(float amount);
    void Pan(float x, float y);
    icSceneView View(int width, int height) const;

  private:
    float center[3] = {}, radius = 1, distance = 3, yaw = .3f, pitch = .12f;
};
