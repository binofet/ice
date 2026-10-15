#include "ice/animation/scene_animation.h"
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
int         main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    std::string              root = argv[1], diagnostics;
    ice::assets::scene_asset scene;
    ice::animation::pose     rest, pose, other;
    for (const char *file : {"SkinProbe.gltf", "SkinProbe-embedded.gltf", "SkinProbe.glb"}) {
        CHECK(ice::assets::load_gltf(root + "/" + file, scene, diagnostics));
        CHECK(scene.skins.size() == 2 && scene.animations.size() == 6 &&
		              scene.primitives.size() == 1 && scene.instances.size() == 2);
        CHECK(close(scene.primitives[0].vertices[0].weights[0], .25f));
        CHECK(ice::animation::evaluate_pose(scene, -1, 0, false, rest, diagnostics));
        CHECK(close(ice::math::element(rest.palettes[0][0], 12), -1) &&
		              close(ice::math::element(rest.palettes[0][1], 12), -1));
        CHECK(close(ice::math::element(rest.world[3], 12), 0) &&
		              close(ice::math::element(rest.world[3], 13), 1));
        CHECK(close(scene.bounds_min[0], -1.4f) && close(scene.bounds_max[1], 1.6f));
        CHECK(ice::animation::evaluate_pose(scene, 0, 1, false, pose, diagnostics));
        CHECK(close(ice::math::element(pose.world[1], 12), 1) &&
		              close(ice::math::element(pose.world[3], 12), 1));
        CHECK(ice::animation::evaluate_pose(scene, 0, 2, false, other, diagnostics));
        CHECK(close(ice::math::element(other.palettes[0][1], 12), 1) &&
		              close(ice::math::element(other.palettes[1][1], 12), -1));
        CHECK(close(ice::math::element(pose.world[1], 12),
		                    1)); // second instance does not modify the first
        float low[3], high[3];
        CHECK(ice::animation::scene_pose_bounds(scene, other, low, high, diagnostics));
        CHECK(close(low[0], -.9f) && close(high[0], .9f));
        CHECK(ice::animation::evaluate_pose(scene, 0, -1, true, pose, diagnostics) &&
		              close(ice::math::element(pose.world[1], 12), 1));
        CHECK(ice::animation::evaluate_pose(scene, 0, -1, false, pose, diagnostics) &&
		              close(ice::math::element(pose.world[1], 12), 0));
        CHECK(ice::animation::evaluate_pose(scene, 0, 2, true, pose, diagnostics) &&
		              close(ice::math::element(pose.world[1], 12), 0));
        CHECK(ice::animation::evaluate_pose(scene, 0, 200, false, pose, diagnostics) &&
		              close(ice::math::element(pose.world[1], 12), 2));
        CHECK(ice::animation::evaluate_pose(scene, 1, 1.99, false, pose, diagnostics) &&
		              close(ice::math::element(pose.local[1], 0), 1));
        CHECK(ice::animation::evaluate_pose(scene, 1, 2, false, pose, diagnostics) &&
		              close(ice::math::element(pose.local[1], 0), 2));
        CHECK(ice::animation::evaluate_pose(scene, 2, 1, false, pose, diagnostics) &&
		              close(ice::math::element(pose.local[1], 12), 1.5f));
        CHECK(ice::animation::evaluate_pose(scene, 3, 1, false, pose, diagnostics) &&
		              close(ice::math::element(pose.local[1], 0), std::sqrt(.5f)) &&
		              close(ice::math::element(pose.local[1], 1), std::sqrt(.5f)));
        CHECK(ice::animation::evaluate_pose(scene, 4, 1, false, pose, diagnostics) &&
		              close(ice::math::element(pose.local[1], 0), 0) &&
		              close(ice::math::element(pose.local[1], 1), 1));
        CHECK(ice::animation::evaluate_pose(scene, 5, 100, true, pose, diagnostics) &&
		              close(ice::math::element(pose.local[1], 12), 4));
    }
    ice::animation::pose a, b, mixed;
    CHECK(ice::animation::evaluate_pose(scene, 0, 2, false, a, diagnostics));
    CHECK(ice::animation::evaluate_pose(scene, 3, 2, false, b, diagnostics));
    CHECK(ice::animation::blend_poses(scene, a, b, 0, mixed, diagnostics) &&
	              mixed.world == a.world);
    CHECK(ice::animation::blend_poses(scene, a, b, 1, mixed, diagnostics) &&
	              mixed.world == b.world);
    CHECK(ice::animation::blend_poses(scene, a, b, .5f, mixed, diagnostics));
    CHECK(close(ice::math::element(mixed.local[1], 12), 2) &&
	              close(ice::math::element(mixed.local[1], 0), std::sqrt(.5f)));
    CHECK(close(ice::math::element(mixed.world[3], 12), 1 - std::sqrt(.5f)) &&
	              close(ice::math::element(mixed.world[3], 13), std::sqrt(.5f)));
    CHECK(close(ice::math::element(mixed.palettes[0][1], 12), 1 - std::sqrt(.5f)));
    CHECK(close(ice::math::element(mixed.local[1], 0) * ice::math::element(mixed.local[1], 0) +
	                        ice::math::element(mixed.local[1], 1) * ice::math::element(mixed.local[1], 1),
	                    1));
    auto snapshot = mixed;
    CHECK(ice::animation::blend_poses(scene, snapshot, a, 0, mixed, diagnostics) &&
	              mixed.world == snapshot.world);
    CHECK(ice::animation::blend_poses(scene, mixed, a, .5f, mixed, diagnostics)); // output alias
    auto blended = mixed.world;
    for (float invalid : {-.1f, 1.1f, std::numeric_limits<float>::quiet_NaN()})
        CHECK(!ice::animation::blend_poses(scene, a, b, invalid, mixed, diagnostics) &&
		              mixed.world == blended);
    ice::animation::pose empty;
    CHECK(!ice::animation::blend_poses(scene, empty, b, .5f, mixed, diagnostics) &&
	              mixed.world == blended);
    CHECK(ice::animation::evaluate_pose(scene, -1, 0, false, a, diagnostics));
    CHECK(ice::animation::evaluate_pose(scene, 1, 2, false, b, diagnostics));
    CHECK(ice::animation::blend_poses(scene, a, b, .5f, mixed, diagnostics));
    CHECK(close(ice::math::element(mixed.local[1], 0), 1.5f) &&
	              close(ice::math::element(mixed.local[1], 12), 1)); // unanimated properties use rest TRS
    auto matrixScene                                   = scene;
    matrixScene.nodes[3].matrix_transform              = true;
    matrixScene.nodes[3].local                         = ice::animation::identity_matrix();
    ice::math::element(matrixScene.nodes[3].local, 13) = 2;
    CHECK(ice::animation::evaluate_pose(matrixScene, 0, 2, false, a, diagnostics));
    CHECK(ice::animation::evaluate_pose(matrixScene, 3, 2, false, b, diagnostics));
    CHECK(ice::animation::blend_poses(matrixScene, a, b, .5f, mixed, diagnostics));
    CHECK(mixed.local[3] == matrixScene.nodes[3].local);
    ice::assets::scene_asset identity;
    CHECK(ice::assets::load_gltf(root + "/identity-binds.gltf", identity, diagnostics));
    CHECK(identity.skins[0].inverse_bind[1] == ice::animation::identity_matrix());
    auto flip = scene;
    for (unsigned i = 4; i < 8; i++)
        flip.animations[3].tracks[0].values[i] *= -1;
    CHECK(ice::animation::evaluate_pose(flip, 3, 1, false, pose, diagnostics) &&
	              close(ice::math::element(pose.local[1], 1), std::sqrt(.5f)));
    auto previous = pose.world;
    CHECK(!ice::animation::evaluate_pose(scene, 100, 0, true, pose, diagnostics) &&
	              pose.world == previous);
    CHECK(!ice::animation::evaluate_pose(scene, 0, std::numeric_limits<double>::infinity(), true,
	                                             pose, diagnostics) &&
	              pose.world == previous);
    auto source = scene.source;
    for (const char *bad : {"duplicate-channel", "matrix-target", "morph-channel", "bad-bind-count",
	                                "eight-influences", "bad-times", "zero-weights", "bad-joint"}) {
        CHECK(!ice::assets::load_gltf(root + "/" + bad + ".gltf", scene, diagnostics) &&
		              scene.source == source && !diagnostics.empty());
    }
    CHECK(ice::assets::load_gltf(argv[2], scene, diagnostics));
    CHECK(scene.animations.size() == 3 && scene.skins.size() == 1);
    for (unsigned clip = 0; clip < scene.animations.size(); clip++) {
        CHECK(ice::animation::evaluate_pose(scene, int(clip), 0, false, rest, diagnostics));
        CHECK(ice::animation::evaluate_pose(scene, int(clip), .2, false, pose, diagnostics));
        CHECK(rest.world != pose.world);
    }
    std::puts("Animation passed: TRS hierarchy/attachments, independent poses, inverse binds, "
	                          "normalized weights, mirrored shared mesh, STEP/SLERP/CUBICSPLINE, clip "
	                          "timing/loop/clamp, TRS pose blending/endpoints/aliases/matrix nodes, malformed "
	                          "inputs and Fox clips");
}
