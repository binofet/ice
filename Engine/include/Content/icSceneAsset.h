#pragma once
#include "Content/icResourceURI.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// CPU assets contain no parser objects, graphics handles or backend-specific types.
struct icSceneVertex {
    float position[3] = {}, normal[3] = {0, 1, 0}, tangent[4] = {1, 0, 0, 1};
    float joints[4] = {}, weights[4] = {1, 0, 0, 0};
    float uv[2][2] = {{0, 0}, {0, 0}}, color[4] = {1, 1, 1, 1};
};
struct icSceneImage {
    std::string name;
    int width = 0, height = 0;
    std::vector<unsigned char> rgba;
};
struct icSceneTexture {
    int image = -1, magFilter = 9729, minFilter = 9987, wrapS = 10497, wrapT = 10497;
};
struct icSceneTextureSlot {
    int texture = -1, uvSet = 0;
    float scale = 1;
};
enum class icAlphaMode { Opaque, Mask, Blend };
struct icSceneMaterial {
    std::string name;
    float baseColor[4] = {1, 1, 1, 1}, emissive[3] = {0, 0, 0}, metallic = 1, roughness = 1,
          alphaCutoff = .5f;
    icSceneTextureSlot baseColorMap, metalRoughMap, normalMap, occlusionMap, emissiveMap;
    icAlphaMode alphaMode = icAlphaMode::Opaque;
    bool doubleSided = false, unlit = false;
};
struct icScenePrimitive {
    bool skinned = false;
    std::vector<icSceneVertex> vertices;
    std::vector<std::uint32_t> indices;
    unsigned material = 0;
};
struct icSceneMesh {
    std::string name;
    std::vector<unsigned> primitives;
};
struct icSceneNode {
    std::string name;
    int parent = -1, mesh = -1, skin = -1;
    bool matrixTransform = false;
    std::array<float, 3> translation{{0, 0, 0}}, scale{{1, 1, 1}};
    std::array<float, 4> rotation{{0, 0, 0, 1}};
    std::vector<unsigned> children;
    std::array<float, 16> local, world;
};
struct icSceneInstance {
    unsigned node = 0, primitive = 0;
};
using icSceneMatrix = std::array<float, 16>;
struct icSceneSkin {
    std::string name;
    int skeleton = -1;
    std::vector<unsigned> joints;
    std::vector<icSceneMatrix> inverseBind;
};
enum class icAnimationPath { Translation, Rotation, Scale };
enum class icAnimationInterpolation { Step, Linear, CubicSpline };
struct icSceneAnimationTrack {
    unsigned node = 0;
    icAnimationPath path = icAnimationPath::Translation;
    icAnimationInterpolation interpolation = icAnimationInterpolation::Linear;
    std::vector<float> times;
    // Cubic keys store in-tangent, value, out-tangent; rotations use XYZW.
    std::vector<float> values;
};
struct icSceneAnimationClip {
    std::string name;
    float start = 0, end = 0;
    std::vector<icSceneAnimationTrack> tracks;
};
struct icSceneAsset {
    std::string source;
    std::vector<std::string> dependencies, warnings;
    std::vector<icSceneImage> images;
    std::vector<icSceneTexture> textures;
    std::vector<icSceneMaterial> materials;
    std::vector<icScenePrimitive> primitives;
    std::vector<icSceneMesh> meshes;
    std::vector<icSceneNode> nodes;
    std::vector<icSceneSkin> skins;
    std::vector<icSceneAnimationClip> animations;
    std::vector<unsigned> roots;
    std::vector<icSceneInstance> instances;
    float boundsMin[3] = {}, boundsMax[3] = {};
};
// Transactional: failure leaves output intact. Core glTF 2.0 / GLB, TRS animation and four-weight
// skins. PNG/JPEG, embedded/data/external resources, UV0/1, triangle lists/strips/fans.
bool icLoadGLTF(const std::string &path, icSceneAsset &output, std::string &diagnostics);

bool icLoadGLTF(const std::string &uri, icResourceResolver &, icSceneAsset &output,
                std::string &diagnostics);
