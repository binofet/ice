#include "SampleHost.h"
#include "TestApp.h"
#include "ice/platform/host.h"
#include <cmath>
struct DesktopClient : ice::platform::host_client {
	TestApp    &app;
	bool        smoke;
	std::string directory;
	unsigned    frames = 0;
	ice::result result = IC_OK;
	DesktopClient(TestApp &a, bool s, const std::string &d) : app(a), smoke(s), directory(d) {}
	void on_update(double dt) override { result = app.update_frame(dt); }
	void on_render(const ice::platform::surface &) override {
		if (ICEFAIL(result))
			return;
		bool capture = smoke && frames == 90;
		result       = app.render_frame(!capture);
		if (ICEFAIL(result))
			return;
		if (smoke && glGetError() != GL_NO_ERROR) {
			result = IC_FAIL_GEN;
			return;
		}
		if (capture) {
			if (!sample::capture(app.window(), directory + "/engine-test.ppm"))
				result = IC_FAIL_GEN;
			app.present_frame();
		}
		++frames;
	}
};
static bool captureScene(TestApp &app, const std::string &directory, const std::string &name) {
	for (int i = 0; i < 8; i++) {
		ice::platform::window::pump_events();
		if (ICEFAIL(app.update_frame(1.0 / 60)) || ICEFAIL(app.render_frame(i != 7)))
			return false;
		if (i == 7) {
			if (!sample::capture(app.window(), directory + "/" + name + ".ppm"))
				return false;
			app.present_frame();
		}
	}
	return true;
}
static bool sceneSuite(TestApp &app, const std::string &directory) {
	app.pause_orbit(true);
	for (int mode = 0; mode < 5; mode++) {
		app.debug_mode(mode);
		if (!captureScene(app, directory, "reference-mode-" + std::to_string(mode)))
			return false;
	}
	app.debug_mode(0);
	app.wireframe(true);
	if (!captureScene(app, directory, "reference-wireframe"))
		return false;
	app.wireframe(false);
	for (unsigned clip = 0; clip < app.scene().animations.size(); clip++) {
		double time =
		    double(app.scene().animations[clip].end - app.scene().animations[clip].start) * .35;
		if (!app.animation_sample(int(clip), time))
			return false;
		auto paused = app.pose().world;
		if (!captureScene(app, directory, "animation-" + std::to_string(clip)) ||
		    app.pose().world != paused)
			return false;
	}
	if (!app.scene().animations.empty()) {
		if (!app.animation_sample(0, .2))
			return false;
		auto paused = app.pose().world;
		if (!app.load_model(app.scene().source, false) || app.pose().world != paused)
			return false;
		if (app.scene().animations.size() > 2) {
			if (!app.animation_sample(1, .3))
				return false;
			app.phase_sync(false);
			if (!app.animation_transition(2, .6) || !app.animation_mix(.5f))
				return false;
			auto blend = app.pose().world;
			if (!captureScene(app, directory, "animation-blend") || app.pose().world != blend)
				return false;
			if (!app.load_model(app.scene().source, false) || app.pose().world != blend)
				return false;
			if (!app.animation_transition(0, .6) || !app.animation_mix(0) ||
			    app.pose().world != blend)
				return false;
			if (!app.animation_mix(.5f) ||
			    !captureScene(app, directory, "animation-interrupted-blend"))
				return false;
			auto held = app.pose().world;
			if (app.animation_mix(2) || app.pose().world != held)
				return false;
			app.play_animation(true);
			for (unsigned frame = 0; frame < 8; frame++)
				if (ICEFAIL(app.update_frame(.1)))
					return false;
			if (app.is_fading())
				return false;
			app.phase_sync(true);
			double fromLength = app.scene().animations[1].end - app.scene().animations[1].start;
			double toLength   = app.scene().animations[2].end - app.scene().animations[2].start;
			if (!app.animation_sample(1, fromLength * .25) || !app.animation_transition(2, 0))
				return false;
			ice::animation::pose expected;
			std::string          diagnostics;
			if (!ice::animation::evaluate_pose(app.scene(), 2, toLength * .25, true, expected,
			                                   diagnostics) ||
			    app.pose().world != expected.world)
				return false;
			if (!app.animation_transition(1, .6))
				return false;
			app.play_animation(true);
			double step = double(.1f);
			if (ICEFAIL(app.update_frame(step)))
				return false;
			ice::animation::pose from, to;
			double               targetTime = fromLength * .25 + step;
			if (!ice::animation::evaluate_pose(app.scene(), 2, targetTime / fromLength * toLength,
			                                   true, from, diagnostics) ||
			    !ice::animation::evaluate_pose(app.scene(), 1, targetTime, true, to, diagnostics) ||
			    !ice::animation::blend_poses(app.scene(), from, to, float(step / .6), expected,
			                                 diagnostics))
				return false;
			for (unsigned node = 0; node < expected.world.size(); node++)
				for (unsigned k = 0; k < 16; k++)
					if (std::fabs(ice::math::element(app.pose().world[node], k) -
					              ice::math::element(expected.world[node], k)) > .0001f)
						return false;
			app.play_animation(false);
			if (!app.animation_sample(0, .2))
				return false;
		}
		app.show_skeleton(true);
		if (!captureScene(app, directory, "animation-bones"))
			return false;
		app.show_skeleton(false);
		if (!app.animation_sample(-1, 0) || !captureScene(app, directory, "animation-rest"))
			return false;
		if (!app.animation_sample(0, 0))
			return false;
	}
	if (!app.load_model("assets://project/gltf/MaterialLab/MaterialLab.gltf"))
		return false;
	app.debug_mode(0);
	if (!captureScene(app, directory, "material-lab"))
		return false;
	auto source = app.scene().source;
	if (app.load_model("assets://project/gltf/does-not-exist.glb") || app.scene().source != source)
		return false;
	if (!captureScene(app, directory, "reload-failure-retains-scene"))
		return false;
	if (!app.load_model(source, false))
		return false;
	ice::platform::window_def def;
	app.window()->get_params(&def);
	def.width  = 800;
	def.height = 1000;
	if (ICEFAIL(app.window()->update_params(def)))
		return false;
	if (!captureScene(app, directory, "material-lab-portrait"))
		return false;
	ice::printf("glTF viewer suite passed: inspection, wireframe, external model, transactional "
	            "reload, animation clips/pause/bones/rest, blends/interruption/reload/phase and "
	            "resized HDR surface");
	return true;
}
int main(int argc, char **argv) {
	std::string modelPath, smokeDirectory, suiteDirectory, projectPath;
	bool        smoke = false;
	for (int i = 1; i < argc; i++) {
		if (std::string(argv[i]) == "--project" || std::string(argv[i]) == "--gltf" ||
		    std::string(argv[i]) == "--smoke-test" || std::string(argv[i]) == "--gltf-smoke") {
			std::string option = argv[i];
			if (++i >= argc) {
				std::fprintf(stderr, "%s requires a path\n", option.c_str());
				return 2;
			}
			std::string path = argv[i];
			bool        uri  = path.find("://") != std::string::npos;
			if (!uri) {
#ifdef WIN32
				char absolute[4096];
				if (!GetFullPathNameA(path.c_str(), sizeof(absolute), absolute, nullptr))
					return 2;
				path = absolute;
#else
				if (path.empty() || path[0] != '/') {
					char cwd[4096];
					if (!getcwd(cwd, sizeof(cwd)))
						return 2;
					path = std::string(cwd) + "/" + path;
				}
#endif
			}
			if (option == "--project")
				projectPath = path;
			else if (option == "--gltf")
				modelPath = path;
			else if (option == "--gltf-smoke")
				suiteDirectory = path;
			else {
				smoke          = true;
				smokeDirectory = path;
			}
		}
	}
	sample::initialize();
	TestApp app;
	if (!projectPath.empty())
		app.project_path(projectPath);
	if (!modelPath.empty())
		app.model_path(modelPath);
	if (ICEFAIL(app.init(argc, argv)))
		return 1;
	if (!suiteDirectory.empty())
		return sceneSuite(app, suiteDirectory) ? 0 : 1;
	DesktopClient       client(app, smoke, smokeDirectory);
	ice::platform::host host(client);
	int                 width, height;
	app.window()->get_framebuffer_size(&width, &height);
	ice::platform::surface surface;
	surface.api    = ice::platform::surface_api::open_gl;
	surface.width  = width;
	surface.height = height;
	auto id        = host.add_surface(surface);
	host.set_state(ice::platform::app_state::active);
	bool exercised = false;
	while (!app.window()->should_close()) {
		ice::platform::window::pump_events();
		if (app.window()->should_close())
			break;
		ice::platform::platform_event event;
		while (ice::platform::platform_events::poll(event)) {
			if (!smoke && event.window == app.window()->get_id()) {
				if (event.type == ice::platform::platform_event_type::focus_lost)
					host.set_state(ice::platform::app_state::inactive);
				if (event.type == ice::platform::platform_event_type::focus_gained)
					host.set_state(ice::platform::app_state::active);
			}
		}
		app.window()->get_framebuffer_size(&width, &height);
		host.resize_surface(id, width, height, 1);
		double now =
		    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
		        .count();
		if (smoke && !exercised && client.frames == 60) {
			host.set_state(ice::platform::app_state::background);
			unsigned previous = client.frames;
			host.frame(now + 30);
			if (client.frames != previous) {
				client.result = IC_FAIL_GEN;
				break;
			}
			host.remove_surface(id);
			id = host.add_surface(surface);
			host.set_state(ice::platform::app_state::active);
			exercised = true;
		}
		host.frame(now);
		if (ICEFAIL(client.result))
			break;
		if (smoke && client.frames == 120) {
			ice::printf("EngineTest smoke passed: %u frames; Lua, path, host pause and surface "
			            "recreation checks passed",
			            client.frames);
			break;
		}
		if (smoke || host.state() != ice::platform::app_state::active)
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
	host.stop();
	return ICEFAIL(client.result) || !app.checks_passed() ? 1 : 0;
}
