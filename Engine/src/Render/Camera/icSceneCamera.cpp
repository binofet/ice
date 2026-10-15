#include "Render/Camera/icSceneCamera.h"
#include <algorithm>
#include <cmath>
namespace {
void normalize(float *v) {
    float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (int i = 0; i < 3; i++)
        v[i] /= l;
}
void cross(const float *a, const float *b, float *v) {
    v[0] = a[1] * b[2] - a[2] * b[1];
    v[1] = a[2] * b[0] - a[0] * b[2];
    v[2] = a[0] * b[1] - a[1] * b[0];
}
float dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
} // namespace
void icSceneCamera::Fit(const icSceneAsset &asset) {
    float sum = 0;
    for (int k = 0; k < 3; k++) {
        center[k] = (asset.boundsMin[k] + asset.boundsMax[k]) * .5f;
        float d = (asset.boundsMax[k] - asset.boundsMin[k]) * .5f;
        sum += d * d;
    }
    radius = std::max(.001f, std::sqrt(sum));
    distance = radius * 2.9f;
    yaw = .3f;
    pitch = .12f;
}
void icSceneCamera::Orbit(float x, float y) {
    yaw += x;
    pitch = std::max(-1.5f, std::min(1.5f, pitch + y));
}
void icSceneCamera::Zoom(float amount) {
    distance = std::max(radius * .15f, std::min(radius * 100.f, distance * std::exp(-amount)));
}
void icSceneCamera::Pan(float x, float y) {
    float right[3] = {std::cos(yaw), 0, -std::sin(yaw)};
    for (int k = 0; k < 3; k++)
        center[k] += right[k] * x * distance;
    center[1] += y * distance;
}
icSceneView icSceneCamera::View(int width, int height) const {
    icSceneView view;
    view.width = width;
    view.height = height;
    float aspect = float(std::max(1, width)) / std::max(1, height),
          d = distance * std::max(1.f, 1.f / aspect);
    float eye[3] = {center[0] + d * std::cos(pitch) * std::sin(yaw),
                    center[1] + d * std::sin(pitch),
                    center[2] + d * std::cos(pitch) * std::cos(yaw)};
    float forward[3] = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]},
          up[3] = {0, 1, 0}, right[3], vertical[3];
    normalize(forward);
    cross(forward, up, right);
    normalize(right);
    cross(right, forward, vertical);
    view.view = {{right[0], vertical[0], -forward[0], 0, right[1], vertical[1], -forward[1], 0,
                  right[2], vertical[2], -forward[2], 0, -dot(right, eye), -dot(vertical, eye),
                  dot(forward, eye), 1}};
    float nearPlane = std::max(radius * .001f, d - radius * 1.5f), farPlane = d + radius * 4.f;
    float cot = 1.f / std::tan(3.14159265f / 8);
    view.projection = {{cot / aspect, 0, 0, 0, 0, cot, 0, 0, 0, 0,
                        -(farPlane + nearPlane) / (farPlane - nearPlane), -1, 0, 0,
                        -2 * farPlane * nearPlane / (farPlane - nearPlane), 0}};
    return view;
}
