#include "TestApp.h"
#include "ice/input/input.h"
#include <algorithm>
#include <cmath>
extern "C" {
#include "Lua/lauxlib.h"
#include "Lua/lua.h"
#include "Lua/lualib.h"
}
TestApp::~TestApp() { cleanup(); }
bool TestApp::reload_script() {
	// Use a fresh VM so a failed reload cannot partially change the live scene.
	lua_State *candidate = luaL_newstate();
	if (!candidate)
		return false;
	luaL_openlibs(candidate);
	if (luaL_dofile(candidate, "Resource/scripts/scene.lua") != LUA_OK) {
		ice::warningf("Lua: %s", lua_tostring(candidate, -1));
		lua_close(candidate);
		return false;
	}
	lua_getglobal(candidate, "rotation_speed");
	bool  valid = lua_isnumber(candidate, -1);
	float speed = (float)lua_tonumber(candidate, -1);
	if (!valid || !std::isfinite(speed) || fabs(speed) > 10) {
		lua_close(candidate);
		ice::warning("rotation_speed must be finite and within -10..10");
		return false;
	}
	if (lua_)
		lua_close(lua_);
	lua_   = candidate;
	speed_ = speed;
	return true;
}
ice::result TestApp::init(int argc, char **argv) {
	ice::platform::window_def def = {0, 1280, 800, 0, 0, "ICE glTF Viewer", false};
	window_                       = ice::platform::window::get_window(def);
	if (!window_)
		return IC_FAIL_CREATE_WIND;
	gx_device_         = new ice::render::gl::graphics_device();
	ice::result result = gx_device_->init(window_);
	if (ICEFAIL(result))
		return result;
	gx_device_->set_clear_color(ice::math::color(24, 32, 48));
	content_ = new ice::assets::content_loader(gx_device_, nullptr);
	if (ICEFAIL(result = content_->load("Resource/models/iceCubeSimple.icm", &cube_)))
		return result;
	ice::render::font_params font = {"Arial", 20};
	if (ICEFAIL(result = gx_device_->create_font(&font_, &font)))
		return result;
	lua_checks_ = reload_script();
	if (!lua_checks_)
		return IC_FAIL_GEN;
	if (ICEFAIL(path_.load("Resource/decent.path")))
		return IC_FAIL_GEN;
	route_checks_ =
	    path_.try_plot_route(ice::math::vec3(0, 0, 0), ice::math::vec3(99, 49, 0), route_);
	if (!route_checks_) {
		ice::warning("EngineTest route query failed");
		return IC_FAIL_GEN;
	}
	ice::printf("EngineTest: Lua scene loaded; path has %d waypoints", route_.size);
	for (int i = 1; i < argc; i++)
		if (std::string(argv[i]) == "--legacy")
			legacy_ = true;
	resources_ = std::make_shared<ice::io::resource_resolver>();
	std::string projectError;
	if (!resources_->load_project(project_path_, projectError)) {
		ice::warningf("Project: %s", projectError.c_str());
		return IC_FAIL_GEN;
	}
	if (!legacy_ && !load_model(model_path_))
		return IC_FAIL_GEN;
	window_->show();
	time_.elapsed();
	return ice::platform::application::init(argc, argv);
}
ice::result TestApp::cleanup() {
	if (lua_) {
		lua_close(lua_);
		lua_ = nullptr;
	}
	if (gx_device_) {
		if (renderer_) {
			renderer_->release();
			renderer_.reset();
		}
		if (font_) {
			gx_device_->release_font(font_);
			font_ = nullptr;
		}
		if (cube_) {
			content_->unload(&cube_);
			cube_ = nullptr;
		}
		delete content_;
		content_ = nullptr;
		delete gx_device_;
		gx_device_ = nullptr;
	}
	if (window_) {
		delete window_;
		window_ = nullptr;
	}
	return IC_OK;
}
ice::result TestApp::update() { return update_frame(time_.elapsed()); }
bool        TestApp::load_model(const std::string &path, bool frame) {
    ice::assets::scene_asset candidate;
    std::string              diagnostics;
    auto                     renderer = ice::render::create_scene_renderer_gl(resources_);
    if (!ice::assets::load_gltf(path, *resources_, candidate, diagnostics) ||
        !renderer->upload(candidate, diagnostics)) {
        message_ = diagnostics;
        ice::warningf("glTF: %s", diagnostics.c_str());
        return false;
    }
    int    clip = candidate.animations.empty() ? -1 : 0;
    double time = 0;
    if (!frame && path == model_path_) {
        time = animation_time_;
        if (clip_ >= 0 && clip_ < int(scene_.animations.size()))
            for (unsigned i = 0; i < candidate.animations.size(); i++)
                if (candidate.animations[i].name == scene_.animations[clip_].name)
                    clip = int(i);
    }
    ice::animation::pose pose;
    if (!ice::animation::evaluate_pose(candidate, bind_pose_ ? -1 : clip, time, loop_, pose,
	                                          diagnostics)) {
        message_ = diagnostics;
        return false;
    }
    bool keepFade = false;
    int  outgoing = -1;
    if (!frame && path == model_path_ && fading_ && !bind_pose_) {
        bool compatible = candidate.nodes.size() == scene_.nodes.size();
        if (compatible)
            for (unsigned i = 0; i < candidate.nodes.size(); i++)
                if (candidate.nodes[i].name != scene_.nodes[i].name ||
                    candidate.nodes[i].parent != scene_.nodes[i].parent ||
                    candidate.nodes[i].matrix_transform != scene_.nodes[i].matrix_transform)
                    compatible = false;
        if (outgoing_clip_ >= 0) {
            bool found = false;
            for (unsigned i = 0; i < candidate.animations.size(); i++)
                if (candidate.animations[i].name == scene_.animations[outgoing_clip_].name) {
                    outgoing = int(i);
                    found    = true;
                    break;
                }
            compatible = compatible && found;
        }
        if (compatible) {
            ice::animation::pose source = fade_from_;
            if ((!fade_frozen_ &&
                 !ice::animation::evaluate_pose(candidate, outgoing, outgoing_time_, loop_, source,
			                                           diagnostics)) ||
                !ice::animation::blend_poses(candidate, source, pose,
			                                        float(fade_elapsed_ / fade_duration_), pose,
			                                        diagnostics)) {
                message_ = diagnostics;
                return false;
            }
            keepFade = true;
        }
    }
    fading_         = keepFade;
    outgoing_clip_  = outgoing;
    pose_           = std::move(pose);
    clip_           = clip;
    animation_time_ = time;
    renderer_       = std::move(renderer);
    scene_          = std::move(candidate);
    scene_ready_    = true;
    model_path_     = path;
    material_       = -1;
    message_        = "Loaded " + path;
    if (frame) {
        ice::assets::scene_asset bounds;
        if (ice::animation::scene_pose_bounds(scene_, pose_, bounds.bounds_min, bounds.bounds_max,
		                                             diagnostics))
            camera_.fit(bounds);
        else
            camera_.fit(scene_);
        if (!scene_.animations.empty()) {
            paused_ = true;
            camera_.orbit(.8f, 0);
        }
    }
    for (auto &warning : scene_.warnings)
        ice::warningf("glTF: %s", warning.c_str());
    ice::printf("glTF loaded: %u nodes, %u instances, %u primitives, %u materials, %u images",
	                   (unsigned)scene_.nodes.size(), (unsigned)scene_.instances.size(),
	                   (unsigned)scene_.primitives.size(), (unsigned)scene_.materials.size(),
	                   (unsigned)scene_.images.size());
    return true;
}
bool TestApp::animation_sample(int clip, double seconds) {
	std::string          diagnostics;
	ice::animation::pose pose;
	if (!ice::animation::evaluate_pose(scene_, clip, seconds, loop_, pose, diagnostics))
		return false;
	fading_         = false;
	pose_           = std::move(pose);
	clip_           = clip;
	animation_time_ = seconds;
	bind_pose_      = clip < 0;
	playing_        = false;
	return true;
}
bool TestApp::evaluate_animation(std::string &diagnostics) {
	ice::animation::pose target;
	if (!ice::animation::evaluate_pose(scene_, bind_pose_ ? -1 : clip_, animation_time_, loop_,
	                                   target, diagnostics))
		return false;
	if (fading_ && !bind_pose_) {
		ice::animation::pose source = fade_from_;
		if (!fade_frozen_ && !ice::animation::evaluate_pose(scene_, outgoing_clip_, outgoing_time_,
		                                                    loop_, source, diagnostics))
			return false;
		if (!ice::animation::blend_poses(scene_, source, target,
		                                 float(fade_elapsed_ / fade_duration_), pose_, diagnostics))
			return false;
	} else
		pose_ = std::move(target);
	return true;
}
bool TestApp::animation_transition(int clip, double duration) {
	if (clip < 0 || clip >= int(scene_.animations.size()) || !std::isfinite(duration) ||
	    duration < 0)
		return false;
	double time = 0;
	if (phase_sync_ && loop_ && clip_ >= 0) {
		auto  &from   = scene_.animations[clip_];
		auto  &to     = scene_.animations[clip];
		double length = double(from.end) - from.start;
		if (length > 0) {
			double phase = std::fmod(animation_time_, length) / length;
			if (phase < 0)
				phase += 1;
			time = phase * (double(to.end) - to.start);
		}
	}
	ice::animation::pose target;
	std::string          diagnostics;
	if (!ice::animation::evaluate_pose(scene_, clip, time, loop_, target, diagnostics))
		return false;
	bool interrupted = fading_;
	fade_from_       = pose_;
	outgoing_clip_   = bind_pose_ ? -1 : clip_;
	outgoing_time_   = animation_time_;
	fade_frozen_     = interrupted;
	fade_elapsed_    = 0;
	fade_duration_   = duration;
	fading_          = duration > 0;
	clip_            = clip;
	animation_time_  = time;
	bind_pose_       = false;
	if (!fading_)
		pose_ = std::move(target);
	return true;
}
bool TestApp::animation_mix(float weight) {
	if (!fading_ || !std::isfinite(weight) || weight < 0 || weight > 1)
		return false;
	double previous = fade_elapsed_;
	fade_elapsed_   = weight * fade_duration_;
	std::string diagnostics;
	if (!evaluate_animation(diagnostics)) {
		fade_elapsed_ = previous;
		return false;
	}
	playing_ = false;
	return true;
}
ice::result TestApp::update_frame(double seconds) {
	auto  input = ice::input::input::instance();
	float dt    = ice::math::clamp((float)seconds, 0.0f, .1f);
	if (input->is_pressed(ice::input::key_escape))
		window_->request_close();
	if (input->is_pressed(ice::input::key_space))
		paused_ = !paused_;
	if (input->is_pressed(ice::input::key_l))
		legacy_ = !legacy_;
	if (input->is_pressed(ice::input::key_r)) {
		if (legacy_)
			reload_script();
		else
			load_model(model_path_, false);
	}
	if (input->is_pressed(ice::input::key_tab))
		show_route_ = !show_route_;
	if (!legacy_) {
		if (input->is_pressed(ice::input::key_n))
			debug_mode_ = (debug_mode_ + 1) % 5;
		if (input->is_pressed(ice::input::key_w))
			wireframe_ = !wireframe_;
		if (input->is_pressed(ice::input::key_f)) {
			ice::assets::scene_asset bounds;
			std::string              error;
			if (ice::animation::scene_pose_bounds(scene_, pose_, bounds.bounds_min,
			                                      bounds.bounds_max, error))
				camera_.fit(bounds);
		}
		if (input->is_pressed(ice::input::key_m)) {
			++material_;
			if (material_ >= (int)scene_.materials.size())
				material_ = -1;
		}
		if (input->is_pressed(ice::input::key_c)) {
			preset_              = (preset_ + 1) % 3;
			const char *models[] = {"assets://project/gltf/Fox/Fox.glb",
			                        "assets://project/gltf/DamagedHelmet/DamagedHelmet.glb",
			                        "assets://project/gltf/MaterialLab/MaterialLab.gltf"};
			load_model(models[preset_]);
		}
		if (input->is_pressed(ice::input::key_b))
			skeleton_ = !skeleton_;
		if (input->is_pressed(ice::input::key_p))
			playing_ = !playing_;
		if (input->is_pressed(ice::input::key_o))
			loop_ = !loop_;
		if (input->is_pressed(ice::input::key_t)) {
			bind_pose_ = !bind_pose_;
			fading_    = false;
		}
		if (input->is_pressed(ice::input::key_a) && !scene_.animations.empty()) {
			animation_transition((clip_ + 1) % int(scene_.animations.size()), crossfade_ ? .6 : 0);
		}
		if (input->is_pressed(ice::input::key_g))
			crossfade_ = !crossfade_;
		if (input->is_pressed(ice::input::key_y))
			phase_sync_ = !phase_sync_;
		if (input->is_pressed(ice::input::key_h) && fading_)
			animation_mix(std::max(0.f, float(fade_elapsed_ / fade_duration_) - .1f));
		if (input->is_pressed(ice::input::key_j) && fading_)
			animation_mix(std::min(1.f, float(fade_elapsed_ / fade_duration_) + .1f));
		if (input->is_pressed(ice::input::key_home)) {
			animation_time_ = 0;
			fading_         = false;
		}
		if (input->is_pressed(ice::input::key_up))
			animation_speed_ = std::min(2.f, animation_speed_ + .25f);
		if (input->is_pressed(ice::input::key_down))
			animation_speed_ = std::max(-2.f, animation_speed_ - .25f);
		if (input->is_pressed(ice::input::key_left) || input->is_pressed(ice::input::key_right)) {
			playing_ = false;
			animation_time_ += (input->is_pressed(ice::input::key_right) ? 1. : -1.) / 30;
		}
		if (clip_ >= 0) {
			if (playing_ && !bind_pose_) {
				animation_time_ += dt * animation_speed_;
				if (fading_) {
					outgoing_time_ += dt * animation_speed_;
					fade_elapsed_ = std::min(fade_duration_, fade_elapsed_ + dt);
				}
			}
			auto  &clip   = scene_.animations[clip_];
			double length = double(clip.end) - clip.start;
			if (loop_ && length > 0) {
				animation_time_ = std::fmod(animation_time_, length);
				if (animation_time_ < 0)
					animation_time_ += length;
			} else
				animation_time_ = std::max(0., std::min(length, animation_time_));
		}
		if (fading_ && !fade_frozen_ && phase_sync_ && loop_ && outgoing_clip_ >= 0 && clip_ >= 0) {
			auto  &source       = scene_.animations[outgoing_clip_];
			auto  &target       = scene_.animations[clip_];
			double targetLength = double(target.end) - target.start;
			if (targetLength > 0)
				outgoing_time_ =
				    animation_time_ / targetLength * (double(source.end) - source.start);
		}
		std::string poseError;
		if (!evaluate_animation(poseError)) {
			message_ = poseError;
			playing_ = false;
			ice::warningf("Animation: %s", poseError.c_str());
		}
		if (fading_ && fade_elapsed_ >= fade_duration_ && playing_)
			fading_ = false;
		float x = input->get_axis(ice::input::mouse_x), y = input->get_axis(ice::input::mouse_y);
		if (input->is_down(ice::input::mouse_l))
			camera_.orbit(-x * 3, y * 3);
		if (input->is_down(ice::input::mouse_r) || input->is_down(ice::input::mouse_m))
			camera_.pan(-x, y);
		camera_.zoom(input->get_axis(ice::input::mouse_z) * .15f);
		if (!paused_)
			camera_.orbit(dt * speed_ * .25f, 0);
	}
	if (!paused_)
		angle_ += dt * speed_;
	input->update();
	return IC_OK;
}
ice::result TestApp::render_frame(bool present) {
	int width, height;
	window_->get_client_size(&width, &height);
	if (height <= 0)
		return IC_OK;
	gx_device_->frame_begin();
	if (!legacy_) {
		if (!scene_ready_) {
			if (!load_model(model_path_))
				return IC_FAIL_GEN;
		}
		auto view       = camera_.view(width, height);
		view.debug_mode = debug_mode_;
		view.wireframe  = wireframe_;
		view.material   = material_;
		view.pose       = &pose_;
		view.skeleton   = skeleton_;
		std::string diagnostics;
		if (!renderer_->draw(view, diagnostics)) {
			ice::warningf("Scene renderer: %s", diagnostics.c_str());
			return IC_FAIL_GEN;
		}
		const char *modes[] = {"PBR studio", "Base color", "Normals", "Metal / roughness", "UV0"};
		font_->printf(ice::math::ivec2(24, height - 34), ice::math::color::white,
		              "ICE glTF Viewer | %s | %s", modes[debug_mode_],
		              wireframe_ ? "wireframe" : "solid");
		font_->printf(ice::math::ivec2(24, height - 62), ice::math::color::white,
		              "%u instances | %u meshes | %u materials | %u images",
		              (unsigned)scene_.instances.size(), (unsigned)scene_.meshes.size(),
		              (unsigned)scene_.materials.size(), (unsigned)scene_.images.size());
		if (clip_ >= 0) {
			auto &clip = scene_.animations[clip_];
			font_->printf(ice::math::ivec2(24, height - 90), ice::math::color::white,
			              "%s | %.2f / %.2fs | %+.2fx | %s | %s", clip.name.c_str(),
			              animation_time_, double(clip.end - clip.start), animation_speed_,
			              bind_pose_ ? "rest pose"
			              : playing_ ? "playing"
			                         : "paused",
			              loop_ ? "loop" : "clamp");
			font_->print(ice::math::ivec2(24, height - 118), ice::math::color::white,
			             "P: play | A: clip | Left/Right: scrub | Up/Down: speed | O: loop | T: "
			             "rest | B: bones");
		}
		if (clip_ >= 0) {
			font_->printf(ice::math::ivec2(24, height - 146), ice::math::color::white,
			              "%s | %s | G: fade/cut | Y: phase/restart | H/J: mix",
			              crossfade_ ? "0.6s crossfade" : "hard cuts",
			              phase_sync_ ? "phase sync" : "restart");
			if (fading_) {
				std::string source = fade_frozen_         ? "visible pose"
				                     : outgoing_clip_ < 0 ? "rest"
				                                          : scene_.animations[outgoing_clip_].name;
				font_->printf(ice::math::ivec2(24, height - 174), ice::math::color::white,
				              "%s -> %s | mix %.0f%%", source.c_str(),
				              scene_.animations[clip_].name.c_str(),
				              100 * fade_elapsed_ / fade_duration_);
			}
		}
		if (material_ >= 0)
			font_->printf(ice::math::ivec2(24, height - 202), ice::math::color::white,
			              "Isolated: %s", scene_.materials[material_].name.c_str());
		font_->print(ice::math::ivec2(24, 80), ice::math::color::white,
		             "Drag: orbit | Right drag: pan | Scroll: zoom | F: frame | Space: rotation");
		font_->print(ice::math::ivec2(24, 52), ice::math::color::white,
		             "C: model | N: inspect | M: material | W: wireframe | R: reload | L: legacy");
		font_->print(ice::math::ivec2(24, 24), ice::math::color::white,
		             message_.substr(0, 120).c_str());
		if (present)
			present_frame();
		return IC_OK;
	}
	ice::math::mat4 projection, view, world;
	projection = glm::perspectiveLH_ZO(float(IC_PI / 4), float((ice::math::real)width / height),
	                                   float(1), float(1000));
	view       = ice::math::camera_pose_lh(ice::math::vec3(0, 12, -45), ice::math::vec3(0, 0, 0),
	                                       ice::math::vec3(0, 1, 0));
	gx_device_->set_projection(projection);
	gx_device_->set_view_matrix(view);
	(world)            = ice::math::mat4(1);
	(world)            = glm::rotate(world, angle_, ice::math::vec3(0, 1, 0));
	(world)            = glm::scale(world, ice::math::vec3(8, 8, 8));
	ice::result result = cube_->render(world);
	font_->print(ice::math::ivec2(20, height - 35), ice::math::color::white,
	             "EngineTest | Space: pause | R: reload Lua | Tab: route | Esc: close");
	font_->printf(ice::math::ivec2(20, 25), ice::math::color::white,
	              "Lua speed %.2f | Path check: %d waypoints", speed_, route_.size);
	if (show_route_) {
		gx_device_->set_projection(projection);
		gx_device_->set_view_matrix(view);
		(world) = ice::math::mat4(1);
		gx_device_->set_world_transform(world);
		glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
		glDisable(GL_LIGHTING);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glLineWidth(2);
		glColor3f(.2f, 1, .4f);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < route_.size; ++i)
			glVertex3f((route_.way_points[i].x - 50) * .2f, 0, (route_.way_points[i].y - 25) * .2f);
		glEnd();
		glPopAttrib();
	}
	if (present)
		present_frame();
	return result;
}
