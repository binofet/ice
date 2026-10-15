#ifndef ICE_TEST_APP_H
#define ICE_TEST_APP_H
#include "AI/Pathing/icAStar.h"
#include "Content/icContentLoader.h"
#include "Core/Application/icApplication.h"
#include "Core/GXDevice/icGXDeviceGL.h"
#include "Render/Camera/icSceneCamera.h"
#include "Render/Font/icFont.h"
#include "Render/icModel.h"
#include "Render/icSceneRenderer.h"
#include "Util/icTime.h"
struct lua_State;
class TestApp : public icApplication {
  public:
    ~TestApp() override;
    ICRESULT Init(int argc, char **argv) override;
    ICRESULT Cleanup() override;
    ICRESULT Update() override;
    ICRESULT UpdateFrame(double seconds);
    ICRESULT Render() override { return RenderFrame(true); }
    ICRESULT RenderFrame(bool present);
    void PresentFrame() { m_pGxDevice->FrameEnd(true); }
    icWindow *Window() const { return m_window; }
    bool ChecksPassed() const { return m_luaChecks && m_routeChecks && (m_legacy || m_sceneReady); }
    void ProjectPath(const std::string &path) { m_projectPath = path; }
    void ModelPath(const std::string &path) { m_modelPath = path; }
    bool LoadModel(const std::string &path, bool frame = true);
    void DebugMode(int mode) { m_debugMode = mode; }
    void Wireframe(bool enabled) { m_wireframe = enabled; }
    void PauseOrbit(bool paused) { m_paused = paused; }
    const icSceneAsset &Scene() const { return m_scene; }
    bool AnimationSample(int clip, double seconds);
    void ShowSkeleton(bool show) { m_skeleton = show; }
    const icScenePose &Pose() const { return m_pose; }

  private:
    bool ReloadScript();
    std::shared_ptr<icResourceResolver> m_resources;
    std::string m_projectPath = "Resource/ice.project";
    icSceneAsset m_scene;
    icScenePose m_pose;
    int m_clip = -1;
    double m_animationTime = 0;
    float m_animationSpeed = 1;
    bool m_playing = true, m_loop = true, m_bindPose = false, m_skeleton = false;
    std::unique_ptr<icSceneRenderer> m_renderer;
    icSceneCamera m_camera;
    std::string m_modelPath = "assets://project/gltf/Fox/Fox.glb", m_message;
    bool m_legacy = false, m_sceneReady = false, m_wireframe = false;
    int m_debugMode = 0, m_material = -1, m_preset = 0;
    icWindow *m_window = nullptr;
    icContentLoader *m_content = nullptr;
    icModel *m_cube = nullptr;
    icFontI *m_font = nullptr;
    lua_State *m_lua = nullptr;
    icAStar m_path;
    icRouteBuffer m_route = {};
    icTimer m_time;
    float m_angle = 0, m_speed = .6f;
    bool m_paused = false, m_showRoute = false, m_luaChecks = false, m_routeChecks = false;
};
#endif
