#include "Content/icSceneAnimation.h"
#include <cmath>
#include <cstdio>
#include <limits>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "Animation failed %d: %s (%s)\n", __LINE__, #x,                   \
                         diagnostics.c_str());                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static bool close(float a, float b) { return std::fabs(a - b) < .0001f; }
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    std::string root = argv[1], diagnostics;
    icSceneAsset scene;
    icScenePose rest, pose, other;
    for (const char *file : {"SkinProbe.gltf", "SkinProbe-embedded.gltf", "SkinProbe.glb"}) {
        CHECK(icLoadGLTF(root + "/" + file, scene, diagnostics));
        CHECK(scene.skins.size() == 2 && scene.animations.size() == 6 &&
              scene.primitives.size() == 1 && scene.instances.size() == 2);
        CHECK(close(scene.primitives[0].vertices[0].weights[0], .25f));
        CHECK(icEvaluateScenePose(scene, -1, 0, false, rest, diagnostics));
        CHECK(close(rest.palettes[0][0][12], -1) && close(rest.palettes[0][1][12], -1));
        CHECK(close(rest.world[3][12], 0) && close(rest.world[3][13], 1));
        CHECK(close(scene.boundsMin[0], -1.4f) && close(scene.boundsMax[1], 1.6f));
        CHECK(icEvaluateScenePose(scene, 0, 1, false, pose, diagnostics));
        CHECK(close(pose.world[1][12], 1) && close(pose.world[3][12], 1));
        CHECK(icEvaluateScenePose(scene, 0, 2, false, other, diagnostics));
        CHECK(close(other.palettes[0][1][12], 1) && close(other.palettes[1][1][12], -1));
        CHECK(close(pose.world[1][12], 1)); // second instance does not modify the first
        float low[3], high[3];
        CHECK(icScenePoseBounds(scene, other, low, high, diagnostics));
        CHECK(close(low[0], -.9f) && close(high[0], .9f));
        CHECK(icEvaluateScenePose(scene, 0, -1, true, pose, diagnostics) &&
              close(pose.world[1][12], 1));
        CHECK(icEvaluateScenePose(scene, 0, -1, false, pose, diagnostics) &&
              close(pose.world[1][12], 0));
        CHECK(icEvaluateScenePose(scene, 0, 2, true, pose, diagnostics) &&
              close(pose.world[1][12], 0));
        CHECK(icEvaluateScenePose(scene, 0, 200, false, pose, diagnostics) &&
              close(pose.world[1][12], 2));
        CHECK(icEvaluateScenePose(scene, 1, 1.99, false, pose, diagnostics) &&
              close(pose.local[1][0], 1));
        CHECK(icEvaluateScenePose(scene, 1, 2, false, pose, diagnostics) &&
              close(pose.local[1][0], 2));
        CHECK(icEvaluateScenePose(scene, 2, 1, false, pose, diagnostics) &&
              close(pose.local[1][12], 1.5f));
        CHECK(icEvaluateScenePose(scene, 3, 1, false, pose, diagnostics) &&
              close(pose.local[1][0], std::sqrt(.5f)) && close(pose.local[1][1], std::sqrt(.5f)));
        CHECK(icEvaluateScenePose(scene, 4, 1, false, pose, diagnostics) &&
              close(pose.local[1][0], 0) && close(pose.local[1][1], 1));
        CHECK(icEvaluateScenePose(scene, 5, 100, true, pose, diagnostics) &&
              close(pose.local[1][12], 4));
    }
    icSceneAsset identity;
    CHECK(icLoadGLTF(root + "/identity-binds.gltf", identity, diagnostics));
    CHECK(identity.skins[0].inverseBind[1] == icSceneIdentity());
    auto flip = scene;
    for (unsigned i = 4; i < 8; i++)
        flip.animations[3].tracks[0].values[i] *= -1;
    CHECK(icEvaluateScenePose(flip, 3, 1, false, pose, diagnostics) &&
          close(pose.local[1][1], std::sqrt(.5f)));
    auto previous = pose.world;
    CHECK(!icEvaluateScenePose(scene, 100, 0, true, pose, diagnostics) && pose.world == previous);
    CHECK(!icEvaluateScenePose(scene, 0, std::numeric_limits<double>::infinity(), true, pose,
                               diagnostics) &&
          pose.world == previous);
    auto source = scene.source;
    for (const char *bad : {"duplicate-channel", "matrix-target", "morph-channel", "bad-bind-count",
                            "eight-influences", "bad-times", "zero-weights", "bad-joint"}) {
        CHECK(!icLoadGLTF(root + "/" + bad + ".gltf", scene, diagnostics) &&
              scene.source == source && !diagnostics.empty());
    }
    CHECK(icLoadGLTF(argv[2], scene, diagnostics));
    CHECK(scene.animations.size() == 3 && scene.skins.size() == 1);
    for (unsigned clip = 0; clip < scene.animations.size(); clip++) {
        CHECK(icEvaluateScenePose(scene, int(clip), 0, false, rest, diagnostics));
        CHECK(icEvaluateScenePose(scene, int(clip), .2, false, pose, diagnostics));
        CHECK(rest.world != pose.world);
    }
    std::puts("Animation passed: TRS hierarchy/attachments, independent poses, inverse binds, "
              "normalized weights, mirrored shared mesh, STEP/SLERP/CUBICSPLINE, clip "
              "timing/loop/clamp, malformed inputs and Fox clips");
}
