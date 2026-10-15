#include "GameModel.h"
#include "SampleHost.h"
#include "ice/input/input.h"
#include "ice/render/font.h"
#include "ice/render/gl/graphics_device.h"
#include <memory>
using namespace playground;
static void box(float x, float y, float z, float sx, float sy, float sz, float r, float g,
                float b) {
	glColor3f(r, g, b);
	glPushMatrix();
	glTranslatef(x, y, z);
	glScalef(sx, sy, sz);
	const float v[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
	                       {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
	const int   f[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
	                       {3, 7, 6, 2}, {1, 2, 6, 5}, {0, 4, 7, 3}};
	glBegin(GL_QUADS);
	for (int i = 0; i < 6; ++i) {
		float light = .55f + i * .08f;
		glColor3f(r * light, g * light, b * light);
		for (int j = 0; j < 4; ++j)
			glVertex3fv(v[f[i][j]]);
	}
	glEnd();
	glPopMatrix();
}
int main(int argc, char **argv) {
	sample::initialize();
	const char *names[] = {"Orbital Salvage", "Warehouse Stealth", "Kart Racer", "Voxel Sandbox"};
	const char *help[]  = {
        "W/S thrust | A/D turn | Space tether nearby cargo | Deliver 3 crates slowly to center",
        "WASD move | Shift sneak | Steal the gold crate, return to green exit | Avoid patrols",
        "W/S throttle / reverse | A/D steer | Visit yellow checkpoints | Complete 3 laps",
        "Arrow keys select column | Q/E layer | Space add/remove block | P save | L load"};
	std::unique_ptr<ice::platform::window> window(
	    ice::platform::window::get_window({0, 1000, 700, 0, 0, names[GAME_MODE], false}));
	if (!window)
		return 1;
	ice::render::gl::graphics_device gx;
	if (ICEFAIL(gx.init(window.get())))
		return 1;
	gx.set_clear_color(ice::math::color(18, 24, 36));
	ice::render::font       *font   = nullptr;
	ice::render::font_params params = {"Arial", 18};
	if (ICEFAIL(gx.create_font(&font, &params)))
		return 1;
	window->show();
	Simulation       game(GAME_MODE);
	ice::util::timer timer;
	auto             input  = ice::input::input::instance();
	unsigned         frames = 0;
	bool             paused = false;
	bool             smoke  = argc == 3 && std::string(argv[1]) == "--smoke-test";
	int              status = 0;
	while (!window->should_close()) {
		ice::platform::window::pump_events();
		if (window->should_close())
			break;
		ice::platform::platform_event event;
		while (ice::platform::platform_events::poll(event)) {
		}
		float dt = smoke ? 1.f / 60 : clamp((float)timer.elapsed(), 0, .05f);
		if (input->is_pressed(ice::input::key_escape))
			window->request_close();
		if (input->is_pressed(ice::input::key_r))
			game.reset();
		if (input->is_pressed(ice::input::key_tab))
			paused = !paused;
		Input controls;
		controls.forward = (input->is_down(ice::input::key_w) ? 1.f : 0) -
		                   (input->is_down(ice::input::key_s) ? 1.f : 0);
		controls.turn = (input->is_down(ice::input::key_d) ? 1.f : 0) -
		                (input->is_down(ice::input::key_a) ? 1.f : 0);
		controls.action = input->is_pressed(ice::input::key_space);
		controls.sneak  = input->is_down(ice::input::key_shift);
		if (GAME_MODE == 3) {
			if (input->is_pressed(ice::input::key_left))
				--game.cursorX;
			if (input->is_pressed(ice::input::key_right))
				++game.cursorX;
			if (input->is_pressed(ice::input::key_up))
				--game.cursorZ;
			if (input->is_pressed(ice::input::key_down))
				++game.cursorZ;
			if (input->is_pressed(ice::input::key_q))
				--game.layer;
			if (input->is_pressed(ice::input::key_e))
				++game.layer;
			game.cursorX = (int)clamp(game.cursorX, 0, 15);
			game.cursorZ = (int)clamp(game.cursorZ, 0, 15);
			game.layer   = (int)clamp(game.layer, 1, 3);
			if (input->is_pressed(ice::input::key_p)) {
				FILE *f = fopen("voxel-world.bin", "wb");
				if (f) {
					fwrite(game.voxels, 1, sizeof(game.voxels), f);
					fclose(f);
				}
			}
			if (input->is_pressed(ice::input::key_l)) {
				FILE *f = fopen("voxel-world.bin", "rb");
				if (f) {
					unsigned char bytes[sizeof(game.voxels)];
					size_t        n = fread(bytes, 1, sizeof(bytes), f);
					fclose(f);
					if (n == sizeof(bytes) &&
					    std::all_of(bytes, bytes + n, [](unsigned char v) { return v <= 1; })) {
						for (size_t i = 0; i < n; ++i)
							reinterpret_cast<bool *>(game.voxels)[i] = bytes[i] != 0;
					}
				}
			}
		}
		if (smoke) {
			controls.forward = .4f;
			controls.turn    = .3f;
			controls.action  = frames == 30;
		}
		if (!paused)
			game.update(dt, controls);
		input->update();
		int w, h;
		window->get_client_size(&w, &h);
		if (h <= 0)
			continue;
		gx.frame_begin();
		ice::math::mat4 projection, view, world;
		projection = glm::perspectiveLH_ZO(float(IC_PI / 4), float((ice::math::real)w / h),
		                                   float(.1f), float(200));
		view       = ice::math::camera_pose_lh(ice::math::vec3(0, 32, 26), ice::math::vec3(0, 0, 0),
		                                       ice::math::vec3(0, 1, 0));
		(world)    = ice::math::mat4(1);
		(world)    = glm::scale(world, ice::math::vec3(-1, 1, 1));
		gx.set_projection(projection);
		gx.set_view_matrix(view);
		gx.set_world_transform(world);
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		if (GAME_MODE == 0) {
			for (int i = 0; i < 60; ++i)
				box((i * 17 % 29) - 14, -1, (i * 11 % 29) - 14, .035f, .035f, .035f, 1, 1, 1);
			box(0, -.3f, 0, 2.5f, .1f, 2.5f, .1f, .7f, .35f);
			for (auto c : game.cargo)
				if (!c.delivered)
					box(c.pos.x, .4f, c.pos.z, .5f, .5f, .5f, 1, .65f, .2f);
			if (game.attached >= 0) {
				glColor3f(.3f, 1, 1);
				glBegin(GL_LINES);
				glVertex3f(game.player.x, .5f, game.player.z);
				auto c = game.cargo[game.attached];
				glVertex3f(c.pos.x, .5f, c.pos.z);
				glEnd();
			}
		} else if (GAME_MODE == 1) {
			box(0, -.3f, 0, 13, .2f, 13, .25f, .3f, .35f);
			box(-4, 1, 0, 1, 1, 6, .6f, .65f, .7f);
			box(4, 1, -3, 1, 1, 6, .6f, .65f, .7f);
			box(-11, 0, 10, 1.5f, .1f, 1.5f, .1f, .8f, .2f);
			if (!game.score)
				box(11, .5f, -11, .6f, .6f, .6f, 1, .8f, .1f);
			for (auto g : game.guards) {
				box(g.x, .5f, g.z, .5f, .7f, .5f, 1, .2f, .2f);
				glColor3f(.5f, .12f, .12f);
				glBegin(GL_LINE_LOOP);
				for (int j = 0; j < 40; ++j) {
					float a = j * IC_PI * 2 / 40;
					glVertex3f(g.x + std::sin(a) * 6, .1f, g.z + std::cos(a) * 6);
				}
				glEnd();
			}
		} else if (GAME_MODE == 2) {
			box(0, -.3f, 0, 13, .2f, 13, .3f, .3f, .3f);
			box(0, -.05f, 0, 7, .1f, 7, .15f, .5f, .18f);
			auto t = game.target();
			box(t.x, .15f, t.z, 1.2f, .15f, 1.2f, 1, .8f, .1f);
		} else {
			for (int x = 0; x < 16; ++x)
				for (int y = 0; y < 4; ++y)
					for (int z = 0; z < 16; ++z)
						if (game.voxels[x][y][z])
							box(x - 7.5f, y, z - 7.5f, .48f, .48f, .48f, y ? .45f : .25f,
							    y ? .65f : .45f, .3f);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			box(game.cursorX - 7.5f, game.layer, game.cursorZ - 7.5f, .53f, .53f, .53f, 1, 1, .1f);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		if (GAME_MODE != 3) {
			glPushMatrix();
			glTranslatef(game.player.x, .7f, game.player.z);
			glRotatef(-game.angle * 180 / IC_PI, 0, 1, 0);
			box(0, 0, 0, .45f, .3f, .8f, .2f, .8f, 1);
			box(0, .1f, -.7f, .2f, .15f, .2f, 1, 1, 1);
			glPopMatrix();
		}
		glPopAttrib();
		font->print(ice::math::ivec2(20, h - 30), ice::math::color::white, names[GAME_MODE]);
		font->print(ice::math::ivec2(20, 45), ice::math::color::white, help[GAME_MODE]);
		font->printf(
		    ice::math::ivec2(20, 20), ice::math::color::white,
		    "R restart | Tab pause | Esc close | Time %.1f | Score %d | Laps %d | Alarm %.1f %s",
		    game.time, game.score, game.laps, game.alarm,
		    game.won    ? "YOU WIN!"
		    : game.lost ? "CAUGHT"
		    : paused    ? "PAUSED"
		                : "");
		if (smoke && frames == 90 &&
		    !sample::capture(window.get(), std::string(argv[2]) + "/playground-" +
		                                       std::to_string(GAME_MODE) + ".ppm")) {
			status = 1;
			break;
		}
		gx.frame_end(true);
		if (smoke && glGetError() != GL_NO_ERROR) {
			status = 1;
			break;
		}
		if (smoke && ++frames == 120) {
			ice::printf("%s smoke passed: %u simulation ticks", names[GAME_MODE], game.ticks);
			break;
		}
	}
	gx.release_font(font);
	return status;
}
