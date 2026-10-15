#include "TestApp.h"
#include "Core/IO/icInput.h"
#include <cmath>
extern "C" {
#include "Lua/lauxlib.h"
#include "Lua/lua.h"
#include "Lua/lualib.h"
}
TestApp::~TestApp() { Cleanup(); }
bool TestApp::ReloadScript() {
    // Use a fresh VM so a failed reload cannot partially change the live scene.
    lua_State *candidate = luaL_newstate();
    if (!candidate)
        return false;
    luaL_openlibs(candidate);
    if (luaL_dofile(candidate, "Resource/scripts/scene.lua") != LUA_OK) {
        icWarningf("Lua: %s", lua_tostring(candidate, -1));
        lua_close(candidate);
        return false;
    }
    lua_getglobal(candidate, "rotation_speed");
    bool valid = lua_isnumber(candidate, -1);
    float speed = (float)lua_tonumber(candidate, -1);
    if (!valid || !std::isfinite(speed) || fabs(speed) > 10) {
        lua_close(candidate);
        icWarning("rotation_speed must be finite and within -10..10");
        return false;
    }
    if (m_lua)
        lua_close(m_lua);
    m_lua = candidate;
    m_speed = speed;
    return true;
}
ICRESULT TestApp::Init(int argc, char **argv) {
    icWindowDef def = {0, 1280, 800, 0, 0, "ICE glTF Viewer", false};
    m_window = icWindow::GetWindow(def);
    if (!m_window)
        return IC_FAIL_CREATE_WIND;
    m_pGxDevice = new icGXDeviceGL();
    ICRESULT result = m_pGxDevice->Init(m_window);
    if (ICEFAIL(result))
        return result;
    m_pGxDevice->SetClearColor(icColor(24, 32, 48));
    m_content = new icContentLoader(m_pGxDevice, nullptr);
    if (ICEFAIL(result = m_content->Load("Resource/models/iceCubeSimple.icm", &m_cube)))
        return result;
    icFontParams font = {"Arial", 20};
    if (ICEFAIL(result = m_pGxDevice->CreateFont(&m_font, &font)))
        return result;
    m_luaChecks = ReloadScript();
    if (!m_luaChecks)
        return IC_FAIL_GEN;
    if (ICEFAIL(m_path.Load("Resource/decent.path")))
        return IC_FAIL_GEN;
    m_routeChecks = m_path.TryPlotRoute(icVector3(0, 0, 0), icVector3(99, 49, 0), m_route);
    if (!m_routeChecks) {
        icWarning("EngineTest route query failed");
        return IC_FAIL_GEN;
    }
    icPrintf("EngineTest: Lua scene loaded; path has %d waypoints", m_route._size);
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--legacy")
            m_legacy = true;
    m_resources = std::make_shared<icResourceResolver>();
    std::string projectError;
    if (!m_resources->LoadProject(m_projectPath, projectError)) {
        icWarningf("Project: %s", projectError.c_str());
        return IC_FAIL_GEN;
    }
    if (!m_legacy && !LoadModel(m_modelPath))
        return IC_FAIL_GEN;
    m_window->Show();
    m_time.Elapsed();
    return icApplication::Init(argc, argv);
}
ICRESULT TestApp::Cleanup() {
    if (m_lua) {
        lua_close(m_lua);
        m_lua = nullptr;
    }
    if (m_pGxDevice) {
        if (m_renderer) {
            m_renderer->Release();
            m_renderer.reset();
        }
        if (m_font) {
            m_pGxDevice->ReleaseFont(m_font);
            m_font = nullptr;
        }
        if (m_cube) {
            m_content->Unload(&m_cube);
            m_cube = nullptr;
        }
        delete m_content;
        m_content = nullptr;
        delete m_pGxDevice;
        m_pGxDevice = nullptr;
    }
    if (m_window) {
        delete m_window;
        m_window = nullptr;
    }
    return IC_OK;
}
ICRESULT TestApp::Update() { return UpdateFrame(m_time.Elapsed()); }
bool TestApp::LoadModel(const std::string &path, bool frame) {
    icSceneAsset candidate;
    std::string diagnostics;
    auto renderer = icCreateSceneRendererGL(m_resources);
    if (!icLoadGLTF(path, *m_resources, candidate, diagnostics) ||
        !renderer->Upload(candidate, diagnostics)) {
        m_message = diagnostics;
        icWarningf("glTF: %s", diagnostics.c_str());
        return false;
    }
    m_renderer = std::move(renderer);
    m_scene = std::move(candidate);
    m_sceneReady = true;
    m_modelPath = path;
    m_material = -1;
    m_message = "Loaded " + path;
    if (frame)
        m_camera.Fit(m_scene);
    for (auto &warning : m_scene.warnings)
        icWarningf("glTF: %s", warning.c_str());
    icPrintf("glTF loaded: %u nodes, %u instances, %u primitives, %u materials, %u images",
             (unsigned)m_scene.nodes.size(), (unsigned)m_scene.instances.size(),
             (unsigned)m_scene.primitives.size(), (unsigned)m_scene.materials.size(),
             (unsigned)m_scene.images.size());
    return true;
}
ICRESULT TestApp::UpdateFrame(double seconds) {
    auto input = icInput::Instance();
    float dt = icClamp((float)seconds, 0.0f, .1f);
    if (input->IsPressed(ICKEY_ESCAPE))
        m_window->RequestClose();
    if (input->IsPressed(ICKEY_SPACE))
        m_paused = !m_paused;
    if (input->IsPressed(ICKEY_L))
        m_legacy = !m_legacy;
    if (input->IsPressed(ICKEY_R)) {
        if (m_legacy)
            ReloadScript();
        else
            LoadModel(m_modelPath, false);
    }
    if (input->IsPressed(ICKEY_TAB))
        m_showRoute = !m_showRoute;
    if (!m_legacy) {
        if (input->IsPressed(ICKEY_N))
            m_debugMode = (m_debugMode + 1) % 5;
        if (input->IsPressed(ICKEY_W))
            m_wireframe = !m_wireframe;
        if (input->IsPressed(ICKEY_F))
            m_camera.Fit(m_scene);
        if (input->IsPressed(ICKEY_M)) {
            ++m_material;
            if (m_material >= (int)m_scene.materials.size())
                m_material = -1;
        }
        if (input->IsPressed(ICKEY_C)) {
            m_preset = (m_preset + 1) % 2;
            LoadModel(m_preset ? "assets://project/gltf/MaterialLab/MaterialLab.gltf"
                               : "assets://project/gltf/DamagedHelmet/DamagedHelmet.glb");
        }
        float x = input->GetAxis(ICAXIS_MOUSE_X), y = input->GetAxis(ICAXIS_MOUSE_Y);
        if (input->IsDown(ICMOUSE_L))
            m_camera.Orbit(-x * 3, y * 3);
        if (input->IsDown(ICMOUSE_R) || input->IsDown(ICMOUSE_M))
            m_camera.Pan(-x, y);
        m_camera.Zoom(input->GetAxis(ICAXIS_MOUSE_Z) * .15f);
        if (!m_paused)
            m_camera.Orbit(dt * m_speed * .25f, 0);
    }
    if (!m_paused)
        m_angle += dt * m_speed;
    input->Update();
    return IC_OK;
}
ICRESULT TestApp::RenderFrame(bool present) {
    int width, height;
    m_window->GetClientSize(&width, &height);
    if (height <= 0)
        return IC_OK;
    m_pGxDevice->FrameBegin();
    if (!m_legacy) {
        if (!m_sceneReady) {
            if (!LoadModel(m_modelPath))
                return IC_FAIL_GEN;
        }
        auto view = m_camera.View(width, height);
        view.debugMode = m_debugMode;
        view.wireframe = m_wireframe;
        view.material = m_material;
        std::string diagnostics;
        if (!m_renderer->Draw(view, diagnostics)) {
            icWarningf("Scene renderer: %s", diagnostics.c_str());
            return IC_FAIL_GEN;
        }
        const char *modes[] = {"PBR studio", "Base color", "Normals", "Metal / roughness", "UV0"};
        m_font->Printf(icVector2Int(24, height - 34), icColor::WHITE, "ICE glTF Viewer | %s | %s",
                       modes[m_debugMode], m_wireframe ? "wireframe" : "solid");
        m_font->Printf(icVector2Int(24, height - 62), icColor::WHITE,
                       "%u instances | %u meshes | %u materials | %u images",
                       (unsigned)m_scene.instances.size(), (unsigned)m_scene.meshes.size(),
                       (unsigned)m_scene.materials.size(), (unsigned)m_scene.images.size());
        if (m_material >= 0)
            m_font->Printf(icVector2Int(24, height - 90), icColor::WHITE, "Isolated: %s",
                           m_scene.materials[m_material].name.c_str());
        m_font->Print(icVector2Int(24, 80), icColor::WHITE,
                      "Drag: orbit | Right drag: pan | Scroll: zoom | F: frame | Space: rotation");
        m_font->Print(icVector2Int(24, 52), icColor::WHITE,
                      "C: model | N: inspect | M: material | W: wireframe | R: reload | L: legacy");
        m_font->Print(icVector2Int(24, 24), icColor::WHITE, m_message.substr(0, 120).c_str());
        if (present)
            PresentFrame();
        return IC_OK;
    }
    icMatrix44 projection, view, world;
    icCreatePerspective(IC_PI / 4, (icReal)width / height, 1, 1000, projection);
    icCreateLookAt(icVector3(0, 12, -45), icVector3::ZERO, icVector3::Y_AXIS, view);
    m_pGxDevice->SetProjection(projection);
    m_pGxDevice->SetViewMatrix(view);
    world.Identity();
    world.RotY(m_angle);
    world.Scale(8, 8, 8);
    ICRESULT result = m_cube->Render(world);
    m_font->Print(icVector2Int(20, height - 35), icColor::WHITE,
                  "EngineTest | Space: pause | R: reload Lua | Tab: route | Esc: close");
    m_font->Printf(icVector2Int(20, 25), icColor::WHITE,
                   "Lua speed %.2f | Path check: %d waypoints", m_speed, m_route._size);
    if (m_showRoute) {
        m_pGxDevice->SetProjection(projection);
        m_pGxDevice->SetViewMatrix(view);
        world.Identity();
        m_pGxDevice->SetWorldTransform(world);
        glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2);
        glColor3f(.2f, 1, .4f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i < m_route._size; ++i)
            glVertex3f((m_route._wayPoints[i].x - 50) * .2f, 0,
                       (m_route._wayPoints[i].y - 25) * .2f);
        glEnd();
        glPopAttrib();
    }
    if (present)
        PresentFrame();
    return result;
}
