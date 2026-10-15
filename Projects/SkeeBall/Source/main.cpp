#include "SampleHost.h"
#include "ice/input/input.h"
#include "skeeball.h"
SkeeBall  app;
SkeeBall &gApp(app);

// Exercise actual surface changes in an isolated sample run, without persisted settings edits.
static ice::result layoutSmoke(const std::string &directory) {
	struct Phase {
		const char *name;
		short       width, height;
		int         state;
		bool        fullscreen;
	};
	const Phase phases[] = {
	    {"title-window", 1280, 720, 0, false},        {"title-ultrawide", 2560, 1080, 0, false},
	    {"settings-ultrawide", 2560, 1080, 1, false}, {"title-compact", 640, 480, 0, false},
	    {"settings-compact", 640, 480, 1, false},     {"game-compact", 640, 480, 2, false},
	    {"pause-compact", 640, 480, 3, false},        {"game-ultrawide", 2560, 1080, 2, false},
	    {"title-fullscreen", 1280, 720, 0, true},     {"settings-fullscreen", 1280, 720, 1, true},
	    {"pause-fullscreen", 1280, 720, 3, true},     {"title-restored", 1280, 720, 0, false}};
	for (const auto &phase : phases) {
		while (app.get_state_machine().peek_state() != &app.main_menu_) {
			if (!app.get_state_machine().pop_state())
				return IC_FAIL_GEN;
		}
		ice::input::input::instance()->cancel_held_input();
		app.main_menu_.on_push();
		ice::platform::window_def def;
		app.get_main_window()->get_params(&def);
		def.width       = phase.width;
		def.height      = phase.height;
		def.full_screen = phase.fullscreen;
		if (ICEFAIL(app.get_main_window()->update_params(def)))
			return IC_FAIL_GEN;
		if (phase.state == 1)
			app.get_state_machine().push_state(&app.settings_menu_);
		if (phase.state >= 2)
			app.get_state_machine().push_state(&app.game_state_);
		if (phase.state == 3)
			app.get_state_machine().push_state(&app.pause_menu_);
		for (int frame = 0; frame < 10; frame++) {
			ice::platform::window::pump_events();
			if (ICEFAIL(app.update()))
				return IC_FAIL_GEN;
			if (ICEFAIL(app.render_frame(frame != 9)) || glGetError() != GL_NO_ERROR)
				return IC_FAIL_GEN;
			if (frame == 9) {
				if (!sample::capture(app.get_main_window(), directory + "/" + phase.name + ".ppm"))
					return IC_FAIL_GEN;
				app.present_frame();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		}
		int w, h;
		app.get_main_window()->get_framebuffer_size(&w, &h);
		ice::printf("Layout checked: %s %dx%d", phase.name, w, h);
		auto key = [](ice::uchar code) {
			ice::input::input::instance()->set_pressed(code, true);
			return app.update();
		};
		if (std::string(phase.name) == "title-ultrawide") {
			// Synthetic input fixture: use client coordinates as native input does.
			int cw, ch;
			app.get_main_window()->get_client_size(&cw, &ch);
			float px = w * .075f + 100, py = h * .48f - 2 * 58 - 24;
			ice::input::input::instance()->update_mouse_position(px * cw / w, ch - py * ch / h, ch);
			if (ICEFAIL(key(ice::input::mouse_l)) ||
			    app.get_state_machine().peek_state() != &app.settings_menu_)
				return IC_FAIL_GEN;
			app.update();
			if (ICEFAIL(key(ice::input::key_escape)) ||
			    app.get_state_machine().peek_state() != &app.main_menu_)
				return IC_FAIL_GEN;
			ice::printf("UI input checked: resized mouse hit-test, settings and back");
		}
		if (std::string(phase.name) == "game-compact") {
			ice::input::input::instance()->set_down(ice::input::key_space, true);
			for (int i = 0; i < 12; i++) {
				app.update();
				std::this_thread::sleep_for(std::chrono::milliseconds(8));
			}
			ice::input::input::instance()->set_down(ice::input::key_space, false);
			app.update();
			if (!app.game_state_.shot_in_flight())
				return IC_FAIL_GEN;
			if (ICEFAIL(key(ice::input::key_escape)) ||
			    app.get_state_machine().peek_state() != &app.pause_menu_)
				return IC_FAIL_GEN;
			app.update();
			key(ice::input::key_down);
			key(ice::input::key_return);
			if (app.get_state_machine().peek_state() != &app.settings_menu_)
				return IC_FAIL_GEN;
			app.update();
			auto settings = app.get_settings();
			key(ice::input::key_right);
			if (app.get_settings().fSfx_vol != std::min(1.f, settings.fSfx_vol + .05f))
				return IC_FAIL_GEN;
			app.update_settings(settings);
			key(ice::input::key_escape);
			if (app.get_state_machine().peek_state() != &app.pause_menu_)
				return IC_FAIL_GEN;
			key(ice::input::key_escape);
			if (app.get_state_machine().peek_state() != &app.game_state_ ||
			    !app.game_state_.shot_in_flight())
				return IC_FAIL_GEN;
			ice::printf(
			    "UI input checked: volume, settings/back/resume preserve an in-flight shot");
		}
	}
	return IC_OK;
}

int main(int argc, char **argv) {
	sample::initialize();
	bool        smoke  = argc == 3 && std::string(argv[1]) == "--smoke-test";
	ice::result result = app.init(argc, argv);
	if (ICEFAIL(result)) {
		ice::warning("SkeeBall initialization failed");
		return 1;
	}
	if (argc == 3 && std::string(argv[1]) == "--layout-smoke") {
		result = layoutSmoke(argv[2]);
		app.cleanup();
		return ICEFAIL(result) ? 1 : 0;
	}
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == "--companion") {
			app.toggle_companion();
			app.get_state_machine().push_state(&app.game_state_);
		}
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == "--browser-controller") {
			app.toggle_browser();
			app.get_state_machine().push_state(&app.game_state_);
		}
	bool pairingSmoke = argc == 3 && std::string(argv[1]) == "--pairing-smoke";
	bool partySmoke   = argc == 3 && std::string(argv[1]) == "--party-smoke";
	if (pairingSmoke)
		app.toggle_companion();
	if (partySmoke)
		app.toggle_browser();
	if (smoke && !std::getenv("ICE_DISABLE_SHADERS")) {
		ice::render::gl::program program;
		std::string              log;
		if (!program.compile("#version 120\nvoid main(){gl_Position=gl_Vertex;}",
		                     "#version 120\nvoid main(){gl_FragColor=vec4(1.0);}", log))
			return 2;
		auto original = program.name();
		if (program.compile("invalid source", "invalid source", log) ||
		    program.name() != original || log.empty())
			return 3;
		ice::printf("GLSL smoke passed: valid link, diagnostic failure, transactional replacement");
	}
	unsigned frame    = 0;
	bool     launched = false;
	while (!app.get_main_window()->should_close()) {
		ice::platform::window::pump_events();
		if (app.get_main_window()->should_close())
			break;
		ice::platform::platform_event event;
		while (ice::platform::platform_events::poll(event)) {
			if (event.window == app.get_main_window()->get_id()) {
				if (event.type == ice::platform::platform_event_type::focus_lost)
					app.CompanionFocused = false;
				if (event.type == ice::platform::platform_event_type::focus_gained)
					app.CompanionFocused = true;
			}
		}
		if (smoke && frame == 30)
			app.get_state_machine().push_state(&app.game_state_);
		if (smoke)
			ice::input::input::instance()->set_down(ice::input::key_space,
			                                        frame >= 50 && frame < 200);
		result = app.update();
		if (ICEFAIL(result))
			break;
		if (smoke && frame == 201)
			launched = app.game_state_.shot_in_flight();
		bool panelCapture = (pairingSmoke || partySmoke) && frame == 30;
		bool capture      = panelCapture || (smoke && (frame == 20 || frame == 240));
		result            = app.render_frame(!capture);
		if (ICEFAIL(result))
			break;
		if (smoke && glGetError() != GL_NO_ERROR) {
			ice::warningf("SkeeBall GL error at frame %u", frame);
			result = IC_FAIL_GEN;
			break;
		}
		if (capture) {
			if (!sample::capture(app.get_main_window(),
			                     panelCapture ? std::string(argv[2])
			                                  : std::string(argv[2]) +
			                                        (frame == 20 ? "/skeeball-menu.ppm"
			                                                     : "/skeeball-game.ppm"))) {
				result = IC_FAIL_GEN;
				break;
			}
			app.present_frame();
			if (panelCapture)
				break;
		}
		++frame;
		if (smoke && frame == 300) {
			if (!launched) {
				ice::warning("Smoke shot did not launch");
				result = IC_FAIL_GEN;
				break;
			}
			ice::printf("SkeeBall smoke passed: %u frames", frame);
			break;
		}
		if (smoke)
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
	}
	app.cleanup();
	return ICEFAIL(result) ? 1 : 0;
}
