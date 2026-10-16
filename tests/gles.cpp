#include "ice/render/device.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "GLES check %d: %s\n", __LINE__, #condition);                     \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
namespace {
using namespace ice::render;
using uniform_data = std::array<std::array<float, 4>, 1024>;
std::array<std::uint8_t, 4> pixel(GLuint texture) {
	GLuint framebuffer = 0;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	std::array<std::uint8_t, 4> result{};
	glReadPixels(31, 15, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
	glDeleteFramebuffers(1, &framebuffer);
	return result;
}
GLuint native_image(int width, int height, GLint format = GL_RGBA8) {
	GLuint value = 0;
	glGenTextures(1, &value);
	glBindTexture(GL_TEXTURE_2D, value);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return value;
}
} // namespace
int main(int argc, char **argv) {
	using namespace ice::render;
	CHECK(argc == 2);
	auto device = device::create(backend_kind::opengles);
	if (!device)
		std::fprintf(stderr, "%s\n", device.error().message.c_str());
	CHECK(device);
	std::printf("Actual GLES driver: %s\n", (*device)->name().c_str());
	auto binding = (*device)->egl_context();
	CHECK(binding && binding->identity && binding->display && binding->context && binding->config);
	ice::io::resource_resolver resolver;
	ice::io::resource_uri      uri;
	std::string                error;
	CHECK(ice::io::resource_uri::from_path(std::string(argv[1]) + "/", uri, error));
	CHECK(resolver.mount("shaders://gles/", uri.string(), error));
	pipeline_options basic;
	basic.vertices   = vertex_layout::position_normal;
	basic.depth_test = basic.depth_write = true;
	basic.sampled_bindings               = 0;
	auto pipeline                        = (*device)->prepare_pipeline(
        resolver, {"shaders://gles/basic.glsl", "vertex_main", "fragment_main"}, basic);
	if (!pipeline)
		std::fprintf(stderr, "%s\n", pipeline.error().message.c_str());
	CHECK(pipeline);
	mesh_data description;
	description.vertices = {
	    {{-1, -1, .5f}, {0, 0, 1}}, {{3, -1, .5f}, {0, 0, 1}}, {{-1, 3, .5f}, {0, 0, 1}}};
	description.indices = {0, 1, 2};
	auto mesh           = (*device)->create_mesh(description);
	CHECK(mesh);
	uniform_data red{}, green{};
	red.back()   = {1, 0, 0, 1};
	green.back() = {0, 1, 0, 1};
	red[0][2]    = .3f;
	green[0][2]  = -.3f;
	const draw_item far{pipeline->get(), mesh->get(), std::as_bytes(std::span(&red, 1))},
	    near{pipeline->get(), mesh->get(), std::as_bytes(std::span(&green, 1))};
	glFrontFace(GL_CW);
	std::array depths{far, near, far};
	auto       captured = (*device)->render_offscreen({depths}, 63, 31);
	CHECK(captured);
	GLint winding = 0;
	glGetIntegerv(GL_FRONT_FACE, &winding);
	CHECK(winding == GL_CCW);
	CHECK(captured->rgba[std::size_t(15 * 63 + 31) * 4 + 1] > 250 && captured->rgba[0] == 0);
	auto bad       = description;
	bad.indices[2] = 100;
	CHECK(!(*device)->create_mesh(bad));
	textured_mesh_data textured;
	skinned_mesh_data  skin;
	for (std::size_t i = 0; i < 3; ++i) {
		const std::array<float, 2> uv{i == 1 ? 2.f : 0.f, i == 2 ? -1.f : 1.f};
		textured.vertices.push_back({description.vertices[i].position, {0, 0, 1}, uv});
		skin.vertices.push_back(
		    {description.vertices[i].position, {0, 0, 1}, uv, {0, 0, 0, 0}, {1, 0, 0, 0}});
	}
	textured.indices = skin.indices = {0, 1, 2};
	auto mesh32 = (*device)->create_mesh(textured), mesh64 = (*device)->create_mesh(skin);
	CHECK(mesh32 && mesh64);
	bad                         = description;
	bad.vertices[0].position[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK(!(*device)->create_mesh(bad));
	auto invalid_skin                  = skin;
	invalid_skin.vertices[0].joints[1] = .5f;
	CHECK(!(*device)->create_mesh(invalid_skin));
	pipeline_options sampled = basic;
	sampled.vertices         = vertex_layout::position_normal_uv;
	sampled.sampled_bindings = 15;
	auto textured_pipeline   = (*device)->prepare_pipeline(
        resolver, {"shaders://gles/textured.glsl", "vertex_main", "fragment_main"}, sampled);
	CHECK(textured_pipeline);
	sampled.vertices      = vertex_layout::position_normal_uv_skin;
	auto skinned_pipeline = (*device)->prepare_pipeline(
	    resolver, {"shaders://gles/skinned.glsl", "vertex_main", "fragment_main"}, sampled);
	CHECK(skinned_pipeline);
	image rows{1, 2, {255, 0, 0, 255, 0, 0, 255, 255}}, black{1, 1, {0, 0, 0, 255}};
	auto  first = (*device)->create_texture(rows), blank = (*device)->create_texture(black);
	CHECK(first && blank);
	uniform_data neutral{};
	draw_item    textured_draw{
	       .pipeline       = textured_pipeline->get(),
	       .geometry       = mesh32->get(),
	       .uniforms       = std::as_bytes(std::span(&neutral, 1)),
	       .sampled_images = {first->get(), blank->get(), blank->get(), blank->get()}};
	auto image32 = (*device)->render_offscreen({std::span(&textured_draw, 1)}, 63, 31);
	CHECK(image32);
	CHECK(image32->rgba[std::size_t(31) * 4] > 55 &&
	      image32->rgba[std::size_t(30 * 63 + 31) * 4 + 2] > 55);
	auto skinned_draw     = textured_draw;
	skinned_draw.pipeline = skinned_pipeline->get();
	skinned_draw.geometry = mesh64->get();
	auto image64          = (*device)->render_offscreen({std::span(&skinned_draw, 1)}, 63, 31);
	CHECK(image64 && image64->rgba == image32->rgba);
	auto target = (*device)->create_render_target({63, 31, color_format::rgba8_unorm});
	CHECK(target && (*device)->render_into(**target, {std::span(&near, 1)}));
	// Native image import is not a runtime acquire/wait implementation. The host supplies proof.
	CHECK((*device)->egl_context());
	GLuint                      external = native_image(63, 31);
	gl::destination_description output{*binding, external, 63, 31, color_format::rgba8_unorm};
	auto                        destination = (*device)->import_destination(output);
	CHECK(destination);
	CHECK(!(*device)->import_destination(output));
	auto lease = (*destination)->begin_acquisition(1);
	CHECK(lease && !lease->waited());
	CHECK(!(*device)->render_into(**destination, *lease, {std::span(&near, 1)}));
	CHECK(!(*destination)->begin_acquisition(2));
	CHECK(lease->generation() == 1 && lease->mark_waited());
	CHECK((*device)->render_into(**destination, *lease, {std::span(&near, 1)}) &&
	      (*device)->flush());
	CHECK(pixel(external)[1] > 250);
	auto other = device::create(backend_kind::opengles);
	CHECK(other);
	CHECK(!(*other)->render_into(**destination, *lease, {std::span(&near, 1)}));
	CHECK(!(*other)->import_destination(output));
	other->reset();
	CHECK((*device)->egl_context());
	CHECK(pixel(external)[1] > 250);
	lease->expire();
	CHECK(!(*device)->render_into(**destination, *lease, {std::span(&far, 1)}));
	CHECK(pixel(external)[1] > 250);
	CHECK(!(*destination)->begin_acquisition(1));
	auto second = (*destination)->begin_acquisition(2);
	CHECK(second && second->mark_waited());
	std::array<std::byte, 16> short_uniform{};
	auto                      short_draw = near;
	short_draw.uniforms                  = short_uniform;
	CHECK(!(*device)->render_into(**destination, *second, {std::span(&short_draw, 1)}) &&
	      pixel(external)[1] > 250);
	GLuint other_eye         = native_image(63, 31);
	auto   other_output      = output;
	other_output.color_image = other_eye;
	auto wrong               = other_output;
	wrong.width              = 62;
	CHECK(!(*device)->import_destination(wrong));
	wrong        = other_output;
	wrong.format = color_format::rgba8_srgb;
	CHECK(!(*device)->import_destination(wrong));
	wrong = other_output;
	wrong.context.identity++;
	CHECK(!(*device)->import_destination(wrong));
	auto eye = (*device)->import_destination(other_output);
	CHECK(eye);
	auto eye_lease = (*eye)->begin_acquisition(1);
	CHECK(eye_lease && eye_lease->mark_waited());
	CHECK(!(*device)->render_into(**destination, *eye_lease, {std::span(&far, 1)}) &&
	      pixel(external)[1] > 250);
	CHECK((*device)->render_into(**eye, *eye_lease, {std::span(&far, 1)}) && (*device)->flush());
	CHECK(pixel(other_eye)[0] > 250);
	eye_lease->expire();
	eye->reset();
	CHECK(glIsTexture(other_eye));
	// A retired wrapper can be rebuilt without deleting or reallocating runtime color.
	eye = (*device)->import_destination(other_output);
	CHECK(eye);
	eye->reset();
	glDeleteTextures(1, &other_eye);
	// Importing a sampled texture as an attachment cannot bypass exact feedback rejection.
	auto owned =
	    (*device)->create_texture(image{63, 31, std::vector<std::uint8_t>(63 * 31 * 4, 255)});
	CHECK(owned);
	GLint owned_name = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &owned_name);
	auto feedback_output        = output;
	feedback_output.color_image = GLuint(owned_name);
	auto feedback               = (*device)->import_destination(feedback_output);
	CHECK(feedback);
	auto feedback_lease = (*feedback)->begin_acquisition(1);
	CHECK(feedback_lease && feedback_lease->mark_waited());
	auto feedback_draw              = textured_draw;
	feedback_draw.sampled_images[0] = owned->get();
	CHECK(!(*device)->render_into(**feedback, *feedback_lease, {std::span(&feedback_draw, 1)}));
	feedback_lease->expire();
	feedback->reset();
	CHECK(glIsTexture(GLuint(owned_name)));
	// Exact sRGB destination contract: linear quarter intensity is encoded once by hardware.
	GLuint srgb             = native_image(63, 31, GL_SRGB8_ALPHA8);
	auto   srgb_output      = output;
	srgb_output.color_image = srgb;
	srgb_output.format      = color_format::rgba8_srgb;
	auto srgb_destination   = (*device)->import_destination(srgb_output);
	CHECK(srgb_destination);
	auto srgb_options          = basic;
	srgb_options.target_format = color_format::rgba8_srgb;
	auto srgb_pipeline         = (*device)->prepare_pipeline(
        resolver, {"shaders://gles/basic.glsl", "vertex_main", "fragment_main"}, srgb_options);
	CHECK(srgb_pipeline);
	uniform_data quarter{};
	quarter.back()     = {.25f, .25f, .25f, 1};
	auto srgb_draw     = near;
	srgb_draw.pipeline = srgb_pipeline->get();
	srgb_draw.uniforms = std::as_bytes(std::span(&quarter, 1));
	auto srgb_lease    = (*srgb_destination)->begin_acquisition(1);
	CHECK(srgb_lease && srgb_lease->mark_waited());
	CHECK(!(*device)->render_into(**srgb_destination, *srgb_lease, {std::span(&near, 1)}));
	CHECK((*device)->render_into(**srgb_destination, *srgb_lease, {std::span(&srgb_draw, 1)}) &&
	      (*device)->flush());
	const auto encoded = pixel(srgb);
	CHECK(encoded[0] >= 135 && encoded[0] <= 139);
	srgb_lease->expire();
	srgb_destination->reset();
	CHECK(glIsTexture(srgb));
	glDeleteTextures(1, &srgb);
	// Destroying wrappers invalidates leases but leaves runtime color ownership untouched.
	destination->reset();
	CHECK(!second->active() && glIsTexture(external));
	glDeleteTextures(1, &external);
	CHECK(glGetError() == GL_NO_ERROR);
	std::puts("GLES actual native proof:24/32/64-byte indexed meshes,16KiB blocks,four "
	          "textures,depth,top-origin captures and borrowed lease/context/feedback/sRGB "
	          "lifetime checks passed");
}
