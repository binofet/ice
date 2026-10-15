#include "Core/GXDevice/icGXDeviceGL.h"
#include "Render/Shader/icPipelineGL.h"
#include "Render/icSceneRenderer.h"
#include "SampleHost.h"
#include <cmath>
#include <memory>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "Scene GL failed %d: %s (%s)\n", __LINE__, #x,                    \
                         diagnostics.c_str());                                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static bool pixel(float x, float y, int r, int g, int b, int tolerance = 5) {
    unsigned char p[3];
    int px = (int)((x + 3) / 6 * 600), py = (int)((y + 2) / 4 * 400);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, p);
    bool ok = std::abs(int(p[0]) - r) <= tolerance && std::abs(int(p[1]) - g) <= tolerance &&
              std::abs(int(p[2]) - b) <= tolerance;
    if (!ok)
        std::fprintf(stderr, "pixel %.1f %.1f = %d %d %d; expected %d %d %d\n", x, y, p[0], p[1],
                     p[2], r, g, b);
    return ok;
}
int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    sample::initialize();
    std::string diagnostics;
    icWindowDef def = {0, 600, 400, 0, 0, "ICE glTF pixel regression", false};
    std::unique_ptr<icWindow> window(icWindow::GetWindow(def));
    CHECK(window);
    icGXDeviceGL device;
    CHECK(!ICEFAIL(device.Init(window.get())));
    auto vertex = std::make_shared<icShaderGL>(), fragment = std::make_shared<icShaderGL>();
    icShaderSource source;
    source.source = "#version 120\nvoid main(){gl_Position=gl_Vertex;}";
    CHECK(vertex->Compile(source, diagnostics));
    source.stage = icShaderStage::Fragment;
    source.source = "#version 120\nuniform vec4 color;void main(){gl_FragColor=color;}";
    CHECK(fragment->Compile(source, diagnostics));
    icPipelineDesc desc;
    desc.vertex = vertex;
    desc.fragment = fragment;
    icPipelineGL pipeline;
    CHECK(pipeline.Create(desc, diagnostics));
    CHECK(pipeline.Bindings().size() == 1 && pipeline.Bindings()[0].name == "color");
    GLuint valid = pipeline.Name();
    desc.vertex = fragment;
    CHECK(!pipeline.Create(desc, diagnostics) && pipeline.Name() == valid);
    desc.vertex = vertex;
    desc.vertices.stride = 4;
    desc.vertices.attributes = {{icVertexSemantic::Position, icVertexFormat::Float3, 0}};
    CHECK(!pipeline.Create(desc, diagnostics) && pipeline.Name() == valid);
    icSceneAsset scene;
    CHECK(icLoadGLTF(argv[1], scene, diagnostics));
    auto resolver = std::make_shared<icResourceResolver>();
    CHECK(resolver->LoadProject(argv[2], diagnostics));
    auto renderer = icCreateSceneRendererGL(resolver);
    CHECK(renderer->Upload(scene, diagnostics));
    icSceneView view;
    view.width = 600;
    view.height = 400;
    view.view = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -3, 1}};
    view.projection = {
        {1.f / 3, 0, 0, 0, 0, .5f, 0, 0, 0, 0, -2.f / 9.9f, 0, 0, 0, -10.1f / 9.9f, 1}};
    view.debugMode = 1;
    device.FrameBegin();
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(-2, 1, 255, 0, 0));
    CHECK(pixel(0, 1, 0, 0, 255));
    CHECK(pixel(0, -1, 188, 137, 137));
    CHECK(pixel(2, 1, 36, 44, 56));
    GLint program = 0, viewport[4], active = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    CHECK(program == 0 && glIsEnabled(GL_DEPTH_TEST) && viewport[2] == 600 && viewport[3] == 400 &&
          active == GL_TEXTURE0);
    view.debugMode = 3;
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(-2, 1, 102, 51, 0));
    view.debugMode = 2;
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(-2, 1, 218, 128, 218));
    view.width = 320;
    view.height = 200;
    CHECK(renderer->Draw(view, diagnostics));
    view.width = 600;
    view.height = 400;
    view.debugMode = 1;
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(0, -1, 188, 137, 137));
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(icLoadGLTF(argv[3], scene, diagnostics));
    CHECK(renderer->Upload(scene, diagnostics));
    icScenePose rest, animated;
    CHECK(icEvaluateScenePose(scene, -1, 0, false, rest, diagnostics));
    view.pose = &rest;
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(-1, 0, 255, 0, 0) && pixel(1, 1.2f, 255, 0, 0));
    CHECK(icEvaluateScenePose(scene, 0, 2, false, animated, diagnostics));
    view.pose = &animated;
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(.5f, 0, 255, 0, 0) && pixel(-1, 0, 36, 44, 56));
    CHECK(pixel(-.5f, 1.2f, 255, 0, 0) && pixel(1, 1.2f, 36, 44, 56));
    // A shear changes inverse-transpose normals while leaving vertex tangents independent.
    for (auto &matrix : rest.palettes[0])
        matrix[2] = 1;
    view.pose = &rest;
    view.debugMode = 2;
    CHECK(renderer->Draw(view, diagnostics));
    CHECK(pixel(-1, 0, 37, 128, 218));
    auto tooLarge = scene;
    tooLarge.skins[0].joints.resize(65, 0);
    tooLarge.skins[0].inverseBind.resize(65, icSceneIdentity());
    CHECK(!renderer->Upload(tooLarge, diagnostics) &&
          diagnostics.find("64 joints") != std::string::npos);
    view.pose = &animated;
    view.debugMode = 1;
    CHECK(renderer->Draw(view, diagnostics) && pixel(.5f, 0, 255, 0, 0));
    icScenePose invalid;
    view.pose = &invalid;
    CHECK(!renderer->Draw(view, diagnostics));
    view.pose = &animated;
    view.skeleton = true;
    CHECK(renderer->Draw(view, diagnostics) && glGetError() == GL_NO_ERROR);
    renderer->Release();
    pipeline.Release();
    fragment->Release();
    vertex->Release();
    std::puts(
        "Scene GL passed: shader stages/pipeline transaction/reflection/layout, texture "
        "orientation/UV1, mirrored winding, mask, sorted linear alpha, MR channels, normal "
        "map, HDR resize/state restoration, weighted GPU skinning, inverse-transpose skin normals, "
        "mirrored skins, ignored mesh transforms, pose validation and transactional joint limits");
}
