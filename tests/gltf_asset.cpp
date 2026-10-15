#include "Content/icSceneAsset.h"
#include "Render/Camera/icSceneCamera.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "glTF check failed %d: %s (%s)\n", __LINE__, #x,                  \
                         diagnostics.c_str());                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    std::string root = argv[1], diagnostics;
    icSceneAsset scene;
    for (const char *name : {"Interleaved.gltf", "Interleaved-embedded.gltf", "Interleaved.glb"}) {
        CHECK(icLoadGLTF(root + "/Interleaved/" + name, scene, diagnostics));
        CHECK(scene.nodes.size() == 3 && scene.instances.size() == 2 &&
              scene.primitives.size() == 1);
        CHECK(std::fabs(scene.primitives[0].vertices[0].color[0] - 128.f / 255) < .0001f);
        CHECK(scene.boundsMin[0] == -1 && scene.boundsMax[0] == 4 && scene.boundsMin[1] == 3 &&
              scene.boundsMax[1] == 4 && scene.boundsMin[2] == 4);
    }
    auto source = scene.source;
    CHECK(!icLoadGLTF(root + "/missing.gltf", scene, diagnostics));
    CHECK(diagnostics.find("Cannot open") != std::string::npos && scene.source == source);
    for (const char *name :
         {"unsupported.gltf", "truncated.gltf", "cycle.gltf", "animation.gltf"}) {
        CHECK(!icLoadGLTF(root + "/Interleaved/" + name, scene, diagnostics));
        CHECK(!diagnostics.empty() && scene.source == source);
    }
    CHECK(icLoadGLTF(root + "/Sparse/Sparse.gltf", scene, diagnostics));
    CHECK(scene.primitives[0].indices.size() == 3);
    CHECK(scene.primitives[0].vertices[1].position[0] == 1 &&
          scene.primitives[0].vertices[0].normal[2] > .99f);
    for (const char *name : {"RenderProbe.gltf", "RenderProbe-embedded.gltf", "RenderProbe.glb"}) {
        CHECK(icLoadGLTF(root + "/RenderProbe/" + name, scene, diagnostics));
        CHECK(scene.images.size() == 3 && scene.instances.size() == 6);
        CHECK(scene.images[0].rgba[0] == 255 && scene.images[0].rgba[8 + 2] == 255);
        CHECK(scene.materials[2].baseColorMap.uvSet == 1);
    }
    CHECK(icLoadGLTF(argv[2], scene, diagnostics));
    CHECK(scene.images.size() == 5 && scene.instances.size() == 1 &&
          scene.primitives[0].vertices.size() > 1000);
    icSceneCamera camera;
    camera.Fit(scene);
    auto view = camera.View(1280, 720);
    float p[4] = {};
    for (int k = 0; k < 4; k++)
        p[k] = view.view[k] * (scene.boundsMin[0] + scene.boundsMax[0]) * .5f +
               view.view[4 + k] * (scene.boundsMin[1] + scene.boundsMax[1]) * .5f +
               view.view[8 + k] * (scene.boundsMin[2] + scene.boundsMax[2]) * .5f +
               view.view[12 + k];
    CHECK(std::fabs(p[0]) < .001f && std::fabs(p[1]) < .001f && p[2] < 0);
    std::puts("glTF passed: external/data/GLB resources, sparse/interleaved/normalized accessors, "
              "hierarchy, instances, images, transactional diagnostics, RH camera");
}
