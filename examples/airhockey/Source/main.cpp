#include "airhockey.h"
#include "ice/input/input.h"
#include "ice/math/rand.h"
#include "ice/memory/memory.h"
#include "ice/platform/window.h"
#include "ice/render/gl/api.h"
#include "ice/util/time.h"
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <algorithm>
#include <chrono>
#include <limits.h>
#include <string>
#include <thread>
#include <vector>
static bool capture(ice::platform::window *window, const std::string &path) {
	int width, height;
	window->get_framebuffer_size(&width, &height);
	std::vector<unsigned char> pixels(width * height * 3);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	if (std::count_if(pixels.begin(), pixels.end(),
	                  [](unsigned char value) { return value > 20; }) < 128)
		return false;
	FILE *file = fopen(path.c_str(), "wb");
	if (!file)
		return false;
	fprintf(file, "P6\n%d %d\n255\n", width, height);
	for (int row = height - 1; row >= 0; --row)
		fwrite(pixels.data() + row * width * 3, 1, width * 3, file);
	fclose(file);
	return glGetError() == GL_NO_ERROR;
}
struct GameplaySmokeChecks {
	static bool run(GameState &game) {
		auto puck = game.puck_.get_phys();
		puck->SetXForm(b2Vec2(0, .9f), 0);
		game.update_score();
		if (game.get_player1_score() != 1)
			return false;
		game.update_score();
		if (game.get_player1_score() != 1)
			return false;
		game.set_player1_score(WINING_SCORE - 1);
		puck->SetXForm(b2Vec2(0, .9f), 0);
		game.update_score();
		if (game.get_player1_score() != WINING_SCORE)
			return false;
		game.update_score();
		if (game.get_player1_score() != WINING_SCORE)
			return false;
		game.on_push();
		return game.get_player1_score() == 0 && game.get_player2_score() == 0;
	}
};
Airhockey  app;
Airhockey &gApp(app);
int        main(int argc, char *args[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    char executable[4096] = {};
#ifdef __APPLE__
	uint32_t size    = sizeof(executable);
	bool     located = _NSGetExecutablePath(executable, &size) == 0;
#elif defined(WIN32)
	bool located = GetModuleFileNameA(nullptr, executable, sizeof(executable)) > 0;
#else
	ssize_t length  = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
	bool    located = length > 0;
#endif
	if (located) {
		std::string path(executable);
		auto        separator = path.find_last_of("/\\");
#ifdef WIN32
		SetCurrentDirectoryA(path.substr(0, separator).c_str());
#else
		chdir(path.substr(0, separator).c_str());
#endif
	}
	ice::memory::memory::init();
	ice::util::sys_time_init();
	ice::math::rand_auto_seed();
	bool        smoke  = argc == 3 && std::string(args[1]) == "--smoke-test";
	std::string output = smoke ? args[2] : "";
	ice::result result = app.init(argc, args);
	if (ICEFAIL(result)) {
		ice::warningf("Airhockey could not start: %s", ice::get_str(result));
		return 1;
	}
	unsigned frame = 0, pausedSteps = 0;
	while (auto window = app.get_main_window()) {
		if (window->should_close())
			break;
		ice::platform::window::pump_events();
		// Airhockey has one game window; future tools route these events by ID.
		ice::platform::platform_event platformEvent;
		while (ice::platform::platform_events::poll(platformEvent)) {
			if (platformEvent.type ==
			    ice::platform::platform_event_type::application_quit_requested)
				app.quit();
		}
		if (window->should_close())
			break;
		if (smoke && frame == 30) {
			app.game_state_.GameMode = GAME_MODE_SINGLE;
			app.get_state_machine().push_state(&app.game_state_);
			if (!GameplaySmokeChecks::run(app.game_state_)) {
				ice::warning("Scoring smoke check failed");
				result = IC_FAIL_GEN;
				break;
			}
		}
		if (smoke && frame == 300) {
			pausedSteps = app.game_state_.physics_steps();
			app.get_state_machine().push_state(&app.pause_menu_);
		}
		if (smoke && frame == 330) {
			if (app.game_state_.physics_steps() != pausedSteps) {
				result = IC_FAIL_GEN;
				break;
			}
			app.get_state_machine().pop_state();
		}
		if (smoke)
			std::this_thread::sleep_for(std::chrono::milliseconds(8));
		result = app.update();
		if (ICEFAIL(result))
			break;
		bool captureFrame = smoke && (frame == 20 || frame == 290 || frame == 320);
		result            = app.render_frame(!captureFrame);
		if (ICEFAIL(result))
			break;
		GLenum error = glGetError();
		if (smoke && error != GL_NO_ERROR) {
			ice::warningf("GL error at frame %u: 0x%x", frame, error);
			result = IC_FAIL_GEN;
			break;
		}
		if (captureFrame) {
			std::string name = frame == 20 ? "menu" : frame == 290 ? "game" : "pause";
			if (!capture(window, output + "/" + name + ".ppm")) {
				ice::warning("Empty or invalid framebuffer capture");
				result = IC_FAIL_GEN;
				break;
			}
			app.present_frame();
		}
		++frame;
		if (smoke && frame == 390) {
			ice::printf("SMOKE frames=%u physics_steps=%u", frame, app.game_state_.physics_steps());
			if (app.game_state_.physics_steps() < 30 ||
			    app.game_state_.physics_steps() <= pausedSteps)
				result = IC_FAIL_GEN;
			break;
		}
	}
	app.cleanup();
	return ICEFAIL(result) ? 1 : 0;
}
