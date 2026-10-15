#include "SampleHost.h"
#include "ice/render/gl/graphics_device.h"
#include "ice/render/gl/pipeline.h"
#include "ice/render/scene_renderer.h"
#include <cmath>
#include <memory>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "Scene GL failed %d: %s (%s)\n", __LINE__, #x,                    \
			             diagnostics.c_str());                                                     \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
static bool pixel(float x, float y, int r, int g, int b, int tolerance = 5) {
	unsigned char p[3];
	int           px = (int)((x + 3) / 6 * 600), py = (int)((y + 2) / 4 * 400);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, p);
	bool ok = std::abs(int(p[0]) - r) <= tolerance && std::abs(int(p[1]) - g) <= tolerance &&
	          std::abs(int(p[2]) - b) <= tolerance;
	if (!ok)
		std::fprintf(stderr, "pixel %.1f %.1f = %d %d %d; expected %d %d %d\n", x, y, p[0], p[1],
		             p[2], r, g, b);
	return ok;
}
int main(int argc, char **argv) {
	if (argc != 4)
		return 2;
	sample::initialize();
	std::string               diagnostics;
	ice::platform::window_def def = {0, 600, 400, 0, 0, "ICE glTF pixel regression", false};
	std::unique_ptr<ice::platform::window> window(ice::platform::window::get_window(def));
	CHECK(window);
	ice::render::gl::graphics_device device;
	CHECK(!ICEFAIL(device.init(window.get())));
	auto vertex   = std::make_shared<ice::render::gl::shader>(),
	     fragment = std::make_shared<ice::render::gl::shader>();
	ice::render::shader_source source;
	source.source = "#version 120\nvoid main(){gl_Position=gl_Vertex;}";
	CHECK(vertex->compile(source, diagnostics));
	source.stage  = ice::render::shader_stage::fragment;
	source.source = "#version 120\nuniform vec4 color;void main(){gl_FragColor=color;}";
	CHECK(fragment->compile(source, diagnostics));
	ice::render::pipeline_desc desc;
	desc.vertex   = vertex;
	desc.fragment = fragment;
	ice::render::gl::pipeline pipeline;
	CHECK(pipeline.create(desc, diagnostics));
	CHECK(pipeline.bindings().size() == 1 && pipeline.bindings()[0].name == "color");
	GLuint valid = pipeline.name();
	desc.vertex  = fragment;
	CHECK(!pipeline.create(desc, diagnostics) && pipeline.name() == valid);
	desc.vertex              = vertex;
	desc.vertices.stride     = 4;
	desc.vertices.attributes = {
	    {ice::render::vertex_semantic::position, ice::render::vertex_format::float3, 0}};
	CHECK(!pipeline.create(desc, diagnostics) && pipeline.name() == valid);
	ice::assets::scene_asset scene;
	CHECK(ice::assets::load_gltf(argv[1], scene, diagnostics));
	auto resolver = std::make_shared<ice::io::resource_resolver>();
	CHECK(resolver->load_project(argv[2], diagnostics));
	auto renderer = ice::render::create_scene_renderer_gl(resolver);
	CHECK(renderer->upload(scene, diagnostics));
	ice::render::scene_view view;
	view.width      = 600;
	view.height     = 400;
	view.view       = glm::translate(ice::math::mat4(1), ice::math::vec3(0, 0, -3));
	view.projection = glm::orthoRH_NO(-3.f, 3.f, -2.f, 2.f, .1f, 10.f);
	view.debug_mode = 1;
	device.frame_begin();
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(-2, 1, 255, 0, 0));
	CHECK(pixel(0, 1, 0, 0, 255));
	CHECK(pixel(0, -1, 188, 137, 137));
	CHECK(pixel(2, 1, 36, 44, 56));
	GLint program = 0, viewport[4], active = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	CHECK(program == 0 && glIsEnabled(GL_DEPTH_TEST) && viewport[2] == 600 && viewport[3] == 400 &&
	      active == GL_TEXTURE0);
	view.debug_mode = 3;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(-2, 1, 102, 51, 0));
	view.debug_mode = 2;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(-2, 1, 218, 128, 218));
	view.width  = 320;
	view.height = 200;
	CHECK(renderer->draw(view, diagnostics));
	view.width      = 600;
	view.height     = 400;
	view.debug_mode = 1;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(0, -1, 188, 137, 137));
	CHECK(glGetError() == GL_NO_ERROR);
	CHECK(ice::assets::load_gltf(argv[3], scene, diagnostics));
	CHECK(renderer->upload(scene, diagnostics));
	ice::animation::pose rest, animated;
	CHECK(ice::animation::evaluate_pose(scene, -1, 0, false, rest, diagnostics));
	view.pose = &rest;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(-1, 0, 255, 0, 0) && pixel(1, 1.2f, 255, 0, 0));
	CHECK(ice::animation::evaluate_pose(scene, 0, 2, false, animated, diagnostics));
	view.pose = &animated;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(.5f, 0, 255, 0, 0) && pixel(-1, 0, 36, 44, 56));
	CHECK(pixel(-.5f, 1.2f, 255, 0, 0) && pixel(1, 1.2f, 36, 44, 56));
	ice::animation::pose mixed;
	CHECK(ice::animation::blend_poses(scene, rest, animated, .5f, mixed, diagnostics));
	view.pose = &mixed;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(-.25f, 0, 255, 0, 0) && pixel(-1, 0, 36, 44, 56));
	// A shear changes inverse-transpose normals while leaving vertex tangents independent.
	for (auto &matrix : rest.palettes[0])
		ice::math::element(matrix, 2) = 1;
	view.pose       = &rest;
	view.debug_mode = 2;
	CHECK(renderer->draw(view, diagnostics));
	CHECK(pixel(-1, 0, 37, 128, 218));
	auto tooLarge = scene;
	tooLarge.skins[0].joints.resize(65, 0);
	tooLarge.skins[0].inverse_bind.resize(65, ice::animation::identity_matrix());
	CHECK(!renderer->upload(tooLarge, diagnostics) &&
	      diagnostics.find("64 joints") != std::string::npos);
	view.pose       = &animated;
	view.debug_mode = 1;
	CHECK(renderer->draw(view, diagnostics) && pixel(.5f, 0, 255, 0, 0));
	ice::animation::pose invalid;
	view.pose = &invalid;
	CHECK(!renderer->draw(view, diagnostics));
	view.pose     = &animated;
	view.skeleton = true;
	CHECK(renderer->draw(view, diagnostics) && glGetError() == GL_NO_ERROR);
	renderer->release();
	pipeline.release();
	fragment->release();
	vertex->release();
	std::puts(
	    "Scene GL passed: shader stages/pipeline transaction/reflection/layout, texture "
	    "orientation/UV1, mirrored winding, mask, sorted linear alpha, MR channels, normal "
	    "map, HDR resize/state restoration, weighted GPU skinning, inverse-transpose skin normals, "
	    "mirrored skins, ignored mesh transforms, pose validation and transactional joint limits");
}
