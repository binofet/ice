#include "host.h"
#include "ice/assets/primitive_mesh.h"
#include <android/log.h>
#include <android_native_app_glue.h>
#include <cmath>
#include <exception>

namespace {
using namespace ice::render;
using namespace ice::math;
using namespace ice::examples::xr;
class lab_shader final : public ice::io::virtual_scheme_handler {
public:
	ice::io::resource_read_result read(const ice::io::resource_uri &,
	                                   std::vector<unsigned char> &bytes, std::string &) override {
		constexpr std::string_view source = R"(
layout(std140) uniform parameters { mat4 mvp,model; vec4 tint; };
#ifdef ICE_VERTEX_STAGE
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
out vec3 world_normal;
void vertex_main(){gl_Position=mvp*vec4(position,1);gl_Position.z=2.0*gl_Position.z-gl_Position.w;world_normal=mat3(transpose(inverse(model)))*normal;}
#else
in vec3 world_normal;
out vec4 color;
void fragment_main(){float light=.25+.75*max(dot(normalize(world_normal),normalize(vec3(.3,1,.5))),0.0);color=vec4(tint.rgb*light,1);}
#endif
)";
		bytes.assign(source.begin(), source.end());
		return ice::io::resource_read_result::success;
	}
};
class lab_client final : public android_client {
public:
	explicit lab_client(device &gpu) : device_(gpu) {}
	ice::expected<void, diagnostic> prepare(color_format format) override {
		ice::io::resource_resolver resolver;
		resolver.register_handler("lab", std::make_shared<lab_shader>());
		auto pipeline = device_.prepare_pipeline(
		    resolver, {"lab://scene.glsl", "vertex_main", "fragment_main"},
		    {vertex_layout::position_normal, true, true, false, format, 0});
		if (!pipeline)
			return ice::unexpected(pipeline.error());
		pipeline_ = std::move(*pipeline);
		auto data = ice::assets::make_primitive({ice::assets::primitive_kind::box});
		if (!data)
			return ice::unexpected(data.error());
		auto geometry = device_.create_mesh(*data);
		if (!geometry)
			return ice::unexpected(geometry.error());
		geometry_ = std::move(*geometry);
		return {};
	}
	void                                   lifecycle(bool) override {}
	ice::expected<void, diagnostic>        update(const frame_observation &) override { return {}; }
	ice::expected<frame_batch, diagnostic> frame(const camera_view       &camera,
	                                             const frame_observation &observations) override {
		count_   = 0;
		auto add = [&](mat4 model, vec3 tint) {
			auto &uniform     = uniforms_.at(count_);
			uniform           = {camera.projection * camera.view * model, model, vec4(tint, 1)};
			draws_.at(count_) = {pipeline_.get(), geometry_.get(),
			                     std::as_bytes(std::span(&uniform, 1)), nullptr};
			++count_;
		};
		auto cube = [&](vec3 position, vec3 size, vec3 tint, float rotation = 0) {
			add(glm::translate(mat4(1), position) *
			        glm::rotate(mat4(1), rotation, glm::normalize(vec3(.3f, 1, .15f))) *
			        glm::scale(mat4(1), size),
			    tint);
		};
		cube({0, -.1f, -1}, {8, .1f, 8}, {.025f, .045f, .065f});
		for (int x = -1; x <= 1; ++x)
			for (int z = -1; z <= 1; ++z)
				cube({float(x) * 2.05f, -.035f, float(z) * 2.05f - 1}, {2, .03f, 2},
				     {.055f, .09f, .105f});
		cube({0, .35f, -2}, {1.2f, .7f, .7f}, {.05f, .12f, .14f});
		cube({0, 1.2f, -2}, {.4f, .4f, .4f}, {.06f, .85f, .7f},
		     float(std::fmod(observations.predicted_seconds * .35, 6.283185307)));
		for (int side : {-1, 1})
			cube({float(side) * 2, 1, -3}, {.12f, 2, .12f}, {.85f, .5f, .12f});
		const vec3 voice = observations.recording           ? vec3(1, .04f, .03f)
		                   : observations.permission_needed ? vec3(.95f, .55f, .03f)
		                   : observations.clip_saved        ? vec3(.08f, .9f, .3f)
		                                                    : vec3(.1f, .15f, .2f);
		cube({0, .8f, -2}, {.35f, .06f, .06f}, voice);
		for (std::size_t hand = 0; hand < 2; ++hand) {
			const auto &controller = observations.controllers[hand];
			if (!controller.valid)
				continue;
			vec3 color = glm::mix(hand == 0 ? vec3(.08f, .45f, .9f) : vec3(.9f, .45f, .08f),
			                      vec3(1), controller.trigger * .7f);
			if (hand == 0 && observations.recording)
				color = voice;
			add(controller.transform *
			        glm::scale(mat4(1), vec3(.05f + .025f * controller.trigger, .05f, .14f)),
			    color);
		}
		return frame_batch{std::span(draws_).first(count_), {.008f, .015f, .025f, 1}};
	}

private:
	struct alignas(16) parameters {
		mat4 mvp{1}, model{1};
		vec4 tint{1};
	};
	device                    &device_;
	std::unique_ptr<pipeline>  pipeline_;
	std::unique_ptr<mesh>      geometry_;
	std::array<parameters, 17> uniforms_;
	std::array<draw_item, 17>  draws_;
	std::size_t                count_ = 0;
};
} // namespace
void android_main(android_app *app) {
	try {
		auto device = ice::render::device::create(ice::render::backend_kind::opengles);
		if (!device) {
			__android_log_print(ANDROID_LOG_ERROR, "ICE-XR", "%s", device.error().message.c_str());
			ice::examples::xr::finish_android_activity(*app);
			return;
		}
		lab_client client(**device);
		ice::examples::xr::run_android(*app, **device, client);
	} catch (const std::exception &error) {
		__android_log_print(ANDROID_LOG_ERROR, "ICE-XR", "Lab exception: %s", error.what());
		ice::examples::xr::finish_android_activity(*app);
	} catch (...) {
		__android_log_print(ANDROID_LOG_ERROR, "ICE-XR", "Unknown lab exception");
		ice::examples::xr::finish_android_activity(*app);
	}
}
