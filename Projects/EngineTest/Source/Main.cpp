#include "Platform/icHost.h"
#include "SampleHost.h"
#include "TestApp.h"
struct DesktopClient : icHostClient {
    TestApp &app;
    bool smoke;
    std::string directory;
    unsigned frames = 0;
    ICRESULT result = IC_OK;
    DesktopClient(TestApp &a, bool s, const std::string &d) : app(a), smoke(s), directory(d) {}
    void OnUpdate(double dt) override { result = app.UpdateFrame(dt); }
    void OnRender(const icSurface &) override {
        if (ICEFAIL(result))
            return;
        bool capture = smoke && frames == 90;
        result = app.RenderFrame(!capture);
        if (ICEFAIL(result))
            return;
        if (smoke && glGetError() != GL_NO_ERROR) {
            result = IC_FAIL_GEN;
            return;
        }
        if (capture) {
            if (!sample::capture(app.Window(), directory + "/engine-test.ppm"))
                result = IC_FAIL_GEN;
            app.PresentFrame();
        }
        ++frames;
    }
};
static bool captureScene(TestApp &app, const std::string &directory, const std::string &name) {
    for (int i = 0; i < 8; i++) {
        icWindow::PumpEvents();
        if (ICEFAIL(app.UpdateFrame(1.0 / 60)) || ICEFAIL(app.RenderFrame(i != 7)))
            return false;
        if (i == 7) {
            if (!sample::capture(app.Window(), directory + "/" + name + ".ppm"))
                return false;
            app.PresentFrame();
        }
    }
    return true;
}
static bool sceneSuite(TestApp &app, const std::string &directory) {
    app.PauseOrbit(true);
    for (int mode = 0; mode < 5; mode++) {
        app.DebugMode(mode);
        if (!captureScene(app, directory, "reference-mode-" + std::to_string(mode)))
            return false;
    }
    app.DebugMode(0);
    app.Wireframe(true);
    if (!captureScene(app, directory, "reference-wireframe"))
        return false;
    app.Wireframe(false);
    for (unsigned clip = 0; clip < app.Scene().animations.size(); clip++) {
        double time =
            double(app.Scene().animations[clip].end - app.Scene().animations[clip].start) * .35;
        if (!app.AnimationSample(int(clip), time))
            return false;
        auto paused = app.Pose().world;
        if (!captureScene(app, directory, "animation-" + std::to_string(clip)) ||
            app.Pose().world != paused)
            return false;
    }
    if (!app.Scene().animations.empty()) {
        if (!app.AnimationSample(0, .2))
            return false;
        auto paused = app.Pose().world;
        if (!app.LoadModel(app.Scene().source, false) || app.Pose().world != paused)
            return false;
        app.ShowSkeleton(true);
        if (!captureScene(app, directory, "animation-bones"))
            return false;
        app.ShowSkeleton(false);
        if (!app.AnimationSample(-1, 0) || !captureScene(app, directory, "animation-rest"))
            return false;
        if (!app.AnimationSample(0, 0))
            return false;
    }
    if (!app.LoadModel("assets://project/gltf/MaterialLab/MaterialLab.gltf"))
        return false;
    app.DebugMode(0);
    if (!captureScene(app, directory, "material-lab"))
        return false;
    auto source = app.Scene().source;
    if (app.LoadModel("assets://project/gltf/does-not-exist.glb") || app.Scene().source != source)
        return false;
    if (!captureScene(app, directory, "reload-failure-retains-scene"))
        return false;
    if (!app.LoadModel(source, false))
        return false;
    icWindowDef def;
    app.Window()->GetParams(&def);
    def.i16Width = 800;
    def.i16Height = 1000;
    if (ICEFAIL(app.Window()->UpdateParams(def)))
        return false;
    if (!captureScene(app, directory, "material-lab-portrait"))
        return false;
    icPrintf("glTF viewer suite passed: inspection, wireframe, external model, transactional "
             "reload, animation clips/pause/bones/rest and resized HDR surface");
    return true;
}
int main(int argc, char **argv) {
    std::string modelPath, smokeDirectory, suiteDirectory, projectPath;
    bool smoke = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--project" || std::string(argv[i]) == "--gltf" ||
            std::string(argv[i]) == "--smoke-test" || std::string(argv[i]) == "--gltf-smoke") {
            std::string option = argv[i];
            if (++i >= argc) {
                std::fprintf(stderr, "%s requires a path\n", option.c_str());
                return 2;
            }
            std::string path = argv[i];
            bool uri = path.find("://") != std::string::npos;
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
                smoke = true;
                smokeDirectory = path;
            }
        }
    }
    sample::initialize();
    TestApp app;
    if (!projectPath.empty())
        app.ProjectPath(projectPath);
    if (!modelPath.empty())
        app.ModelPath(modelPath);
    if (ICEFAIL(app.Init(argc, argv)))
        return 1;
    if (!suiteDirectory.empty())
        return sceneSuite(app, suiteDirectory) ? 0 : 1;
    DesktopClient client(app, smoke, smokeDirectory);
    icHost host(client);
    int width, height;
    app.Window()->GetFramebufferSize(&width, &height);
    icSurface surface;
    surface.api = icSurfaceAPI::OpenGL;
    surface.width = width;
    surface.height = height;
    auto id = host.AddSurface(surface);
    host.SetState(icAppState::Active);
    bool exercised = false;
    while (!app.Window()->ShouldClose()) {
        icWindow::PumpEvents();
        if (app.Window()->ShouldClose())
            break;
        icPlatformEvent event;
        while (icPlatformEvents::Poll(event)) {
            if (!smoke && event.window == app.Window()->GetID()) {
                if (event.type == icPlatformEventType::FocusLost)
                    host.SetState(icAppState::Inactive);
                if (event.type == icPlatformEventType::FocusGained)
                    host.SetState(icAppState::Active);
            }
        }
        app.Window()->GetFramebufferSize(&width, &height);
        host.ResizeSurface(id, width, height, 1);
        double now =
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        if (smoke && !exercised && client.frames == 60) {
            host.SetState(icAppState::Background);
            unsigned previous = client.frames;
            host.Frame(now + 30);
            if (client.frames != previous) {
                client.result = IC_FAIL_GEN;
                break;
            }
            host.RemoveSurface(id);
            id = host.AddSurface(surface);
            host.SetState(icAppState::Active);
            exercised = true;
        }
        host.Frame(now);
        if (ICEFAIL(client.result))
            break;
        if (smoke && client.frames == 120) {
            icPrintf("EngineTest smoke passed: %u frames; Lua, path, host pause and surface "
                     "recreation checks passed",
                     client.frames);
            break;
        }
        if (smoke || host.State() != icAppState::Active)
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    host.Stop();
    return ICEFAIL(client.result) || !app.ChecksPassed() ? 1 : 0;
}
