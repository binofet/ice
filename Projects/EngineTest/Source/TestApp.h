#ifndef ICE_TEST_APP_H
#define ICE_TEST_APP_H
#include "ice/ai/a_star.h"
#include "ice/assets/content_loader.h"
#include "ice/platform/application.h"
#include "ice/render/font.h"
#include "ice/render/gl/graphics_device.h"
#include "ice/render/model.h"
#include "ice/render/scene_camera.h"
#include "ice/render/scene_renderer.h"
#include "ice/util/time.h"
struct lua_State;
class TestApp : public ice::platform::application {
public:
	~TestApp() override;
	ice::result            init(int argc, char **argv) override;
	ice::result            cleanup() override;
	ice::result            update() override;
	ice::result            update_frame(double seconds);
	ice::result            render() override { return render_frame(true); }
	ice::result            render_frame(bool present);
	void                   present_frame() { gx_device_->frame_end(true); }
	ice::platform::window *window() const { return window_; }
	bool checks_passed() const { return lua_checks_ && route_checks_ && (legacy_ || scene_ready_); }
	void project_path(const std::string &path) { project_path_ = path; }
	void model_path(const std::string &path) { model_path_ = path; }
	bool load_model(const std::string &path, bool frame = true);
	void debug_mode(int mode) { debug_mode_ = mode; }
	void wireframe(bool enabled) { wireframe_ = enabled; }
	void pause_orbit(bool paused) { paused_ = paused; }
	const ice::assets::scene_asset &scene() const { return scene_; }
	bool                            animation_sample(int clip, double seconds);
	bool                            animation_transition(int clip, double duration = .6);
	bool                            animation_mix(float weight);
	void                            phase_sync(bool enabled) { phase_sync_ = enabled; }
	void                            play_animation(bool enabled) { playing_ = enabled; }
	bool                            is_fading() const { return fading_; }
	void                            show_skeleton(bool show) { skeleton_ = show; }
	const ice::animation::pose     &pose() const { return pose_; }

private:
	bool                                        reload_script();
	std::shared_ptr<ice::io::resource_resolver> resources_;
	std::string                                 project_path_ = "Resource/ice.project";
	ice::assets::scene_asset                    scene_;
	ice::animation::pose                        pose_;
	int                                         clip_ = -1;
	ice::animation::pose                        fade_from_;
	int                                         outgoing_clip_ = -1;
	double outgoing_time_ = 0, fade_elapsed_ = 0, fade_duration_ = .6;
	bool   fading_ = false, fade_frozen_ = false, crossfade_ = true, phase_sync_ = true;
	bool   evaluate_animation(std::string &diagnostics);
	double animation_time_  = 0;
	float  animation_speed_ = 1;
	bool   playing_ = true, loop_ = true, bind_pose_ = false, skeleton_ = false;
	std::unique_ptr<ice::render::scene_renderer> renderer_;
	ice::render::scene_camera                    camera_;
	std::string                  model_path_ = "assets://project/gltf/Fox/Fox.glb", message_;
	bool                         legacy_ = false, scene_ready_ = false, wireframe_ = false;
	int                          debug_mode_ = 0, material_ = -1, preset_ = 0;
	ice::platform::window       *window_  = nullptr;
	ice::assets::content_loader *content_ = nullptr;
	ice::render::model          *cube_    = nullptr;
	ice::render::font           *font_    = nullptr;
	lua_State                   *lua_     = nullptr;
	ice::ai::a_star              path_;
	ice::ai::route_buffer        route_ = {};
	ice::util::timer             time_;
	float                        angle_ = 0, speed_ = .6f;
	bool paused_ = false, show_route_ = false, lua_checks_ = false, route_checks_ = false;
};
#endif
