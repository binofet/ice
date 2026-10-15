#include "ice/assets/scene_asset.h"
#include "ice/render/scene_camera.h"
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
	std::string              root = argv[1], diagnostics;
	ice::assets::scene_asset scene;
	for (const char *name : {"Interleaved.gltf", "Interleaved-embedded.gltf", "Interleaved.glb"}) {
		CHECK(ice::assets::load_gltf(root + "/Interleaved/" + name, scene, diagnostics));
		CHECK(scene.nodes.size() == 3 && scene.instances.size() == 2 &&
		      scene.primitives.size() == 1);
		CHECK(std::fabs(scene.primitives[0].vertices[0].color[0] - 128.f / 255) < .0001f);
		CHECK(scene.bounds_min[0] == -1 && scene.bounds_max[0] == 4 && scene.bounds_min[1] == 3 &&
		      scene.bounds_max[1] == 4 && scene.bounds_min[2] == 4);
	}
	auto source = scene.source;
	CHECK(!ice::assets::load_gltf(root + "/missing.gltf", scene, diagnostics));
	CHECK(diagnostics.find("Cannot open") != std::string::npos && scene.source == source);
	for (const char *name :
	     {"unsupported.gltf", "truncated.gltf", "cycle.gltf", "animation.gltf"}) {
		CHECK(!ice::assets::load_gltf(root + "/Interleaved/" + name, scene, diagnostics));
		CHECK(!diagnostics.empty() && scene.source == source);
	}
	CHECK(ice::assets::load_gltf(root + "/Sparse/Sparse.gltf", scene, diagnostics));
	CHECK(scene.primitives[0].indices.size() == 3);
	CHECK(scene.primitives[0].vertices[1].position[0] == 1 &&
	      scene.primitives[0].vertices[0].normal[2] > .99f);
	for (const char *name : {"RenderProbe.gltf", "RenderProbe-embedded.gltf", "RenderProbe.glb"}) {
		CHECK(ice::assets::load_gltf(root + "/RenderProbe/" + name, scene, diagnostics));
		CHECK(scene.images.size() == 3 && scene.instances.size() == 6);
		CHECK(scene.images[0].rgba[0] == 255 && scene.images[0].rgba[8 + 2] == 255);
		CHECK(scene.materials[2].base_color_map.uv_set == 1);
	}
	CHECK(ice::assets::load_gltf(argv[2], scene, diagnostics));
	CHECK(scene.images.size() == 5 && scene.instances.size() == 1 &&
	      scene.primitives[0].vertices.size() > 1000);
	ice::render::scene_camera camera;
	camera.fit(scene);
	auto  view = camera.view(1280, 720);
	float p[4] = {};
	for (int k = 0; k < 4; k++)
		p[k] =
		    ice::math::element(view.view, k) * (scene.bounds_min[0] + scene.bounds_max[0]) * .5f +
		    ice::math::element(view.view, 4 + k) * (scene.bounds_min[1] + scene.bounds_max[1]) *
		        .5f +
		    ice::math::element(view.view, 8 + k) * (scene.bounds_min[2] + scene.bounds_max[2]) *
		        .5f +
		    ice::math::element(view.view, 12 + k);
	CHECK(std::fabs(p[0]) < .001f && std::fabs(p[1]) < .001f && p[2] < 0);
	std::puts("glTF passed: external/data/GLB resources, sparse/interleaved/normalized accessors, "
	          "hierarchy, instances, images, transactional diagnostics, RH camera");
}
