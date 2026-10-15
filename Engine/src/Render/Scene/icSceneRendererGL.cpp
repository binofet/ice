#include "Render/Shader/icPipelineGL.h"
#include "Render/Texture/icRenderTargetGL.h"
#include "Render/icSceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace {
struct MeshBuffers {
    GLuint vertices = 0, indices = 0;
    GLsizei count = 0;
    unsigned material = 0;
};
struct Instance {
    std::array<float, 16> world;
    unsigned primitive = 0, node = 0;
    int skin = -1;
    float localCenter[3] = {};
    float center[3] = {};
    float determinant = 1;
};
float determinant(const float *m) {
    return m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) +
           m[8] * (m[1] * m[6] - m[5] * m[2]);
}
struct Resources {
    std::vector<MeshBuffers> meshes;
    std::vector<GLuint> textures;
    std::vector<icSceneMaterial> materials;
    std::vector<Instance> instances;
    icScenePose rest;
    std::vector<std::pair<unsigned, unsigned>> bones;
    std::vector<unsigned> jointNodes;
    ~Resources() {
        for (auto &m : meshes) {
            if (m.vertices)
                glDeleteBuffers(1, &m.vertices);
            if (m.indices)
                glDeleteBuffers(1, &m.indices);
        }
        if (!textures.empty())
            glDeleteTextures((GLsizei)textures.size(), textures.data());
    }
};
class SceneGL final : public icSceneRenderer {
  public:
    explicit SceneGL(std::shared_ptr<icResourceResolver> r) : resolver(std::move(r)) {}
    bool Upload(const icSceneAsset &, std::string &) override;
    bool Draw(const icSceneView &, std::string &) override;
    void Release() override {
        target.Release();
        composite.Release();
        resources.reset();
        pipelines.clear();
        vertex.reset();
        fragment.reset();
        skinVertex.reset();
    }
    ~SceneGL() { Release(); }

  private:
    std::shared_ptr<icResourceResolver> resolver;
    bool ShaderText(const char *uri, std::string &text, std::string &diagnostics) {
        std::vector<unsigned char> bytes;
        if (!resolver || !resolver->Read(uri, bytes, diagnostics)) {
            if (diagnostics.empty())
                diagnostics = "Shader URI resolver is required";
            return false;
        }
        if (bytes.size() > 1024 * 1024) {
            diagnostics = "Shader source exceeds 1 MiB";
            return false;
        }
        text.assign(bytes.begin(), bytes.end());
        return true;
    }
    bool framebufferSRGB = false;
    icRenderTargetGL target;
    icProgramGL composite;
    std::unique_ptr<Resources> resources;
    std::shared_ptr<icShaderGL> vertex, fragment, skinVertex;
    std::map<unsigned, std::unique_ptr<icPipelineGL>> pipelines;
    icPipelineGL *Pipeline(const icSceneMaterial &, bool mirror, bool wire, bool skinned,
                           std::string &);
};
icPipelineGL *SceneGL::Pipeline(const icSceneMaterial &material, bool mirror, bool wire,
                                bool skinned, std::string &diagnostics) {
    bool blend = material.alphaMode == icAlphaMode::Blend;
    unsigned key = (blend ? 1 : 0) | (material.doubleSided ? 2 : 0) | (mirror ? 4 : 0) |
                   (wire ? 8 : 0) | (skinned ? 16 : 0);
    auto found = pipelines.find(key);
    if (found != pipelines.end())
        return found->second.get();
    icPipelineDesc desc;
    desc.vertex = skinned ? skinVertex : vertex;
    desc.fragment = fragment;
    desc.label = "glTF metallic/roughness";
    desc.colorTarget = icColorTargetFormat::LinearRGBA16Float;
    desc.vertices.stride = sizeof(icSceneVertex);
    desc.vertices.attributes = {
        {icVertexSemantic::Position, icVertexFormat::Float3, offsetof(icSceneVertex, position)},
        {icVertexSemantic::Normal, icVertexFormat::Float3, offsetof(icSceneVertex, normal)},
        {icVertexSemantic::Color0, icVertexFormat::Float4, offsetof(icSceneVertex, color)},
        {icVertexSemantic::TexCoord0, icVertexFormat::Float2, offsetof(icSceneVertex, uv)},
        {icVertexSemantic::TexCoord1, icVertexFormat::Float2,
         offsetof(icSceneVertex, uv) + sizeof(float) * 2},
        {icVertexSemantic::Tangent, icVertexFormat::Float4, offsetof(icSceneVertex, tangent)},
        {icVertexSemantic::Joints0, icVertexFormat::Float4, offsetof(icSceneVertex, joints)},
        {icVertexSemantic::Weights0, icVertexFormat::Float4, offsetof(icSceneVertex, weights)}};
    desc.state.blend = blend ? icBlendMode::Alpha : icBlendMode::Opaque;
    desc.state.depthWrite = !blend;
    desc.state.cull = material.doubleSided ? icCullMode::None : icCullMode::Back;
    desc.state.clockwiseFront = mirror;
    desc.state.wireframe = wire;
    std::unique_ptr<icPipelineGL> pipeline(new icPipelineGL);
    if (!pipeline->Create(desc, diagnostics))
        return nullptr;
    auto pointer = pipeline.get();
    pipelines[key] = std::move(pipeline);
    return pointer;
}
bool SceneGL::Upload(const icSceneAsset &scene, std::string &diagnostics) {
    diagnostics.clear();
    if (!vertex) {
        auto v = std::make_shared<icShaderGL>(), f = std::make_shared<icShaderGL>();
        icShaderSource source;
        source.label = "glTF vertex";
        source.uri = "shaders://project/gltf.vert";
        if (!ShaderText(source.uri.c_str(), source.source, diagnostics) ||
            !v->Compile(source, diagnostics))
            return false;
        source.stage = icShaderStage::Fragment;
        source.label = "glTF PBR";
        source.uri = "shaders://project/gltf.frag";
        if (!ShaderText(source.uri.c_str(), source.source, diagnostics) ||
            !f->Compile(source, diagnostics))
            return false;
        vertex = v;
        fragment = f;
    }
    for (auto &skin : scene.skins)
        if (skin.joints.size() > 64) {
            diagnostics = "GL compatibility skinning supports at most 64 joints per skin";
            return false;
        }
    if (!scene.skins.empty() && !skinVertex) {
        GLint components = 0;
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &components);
        if (components < 64 * 16 + 128) {
            diagnostics = "GPU has insufficient vertex uniforms for 64-joint skinning";
            return false;
        }
        auto stage = std::make_shared<icShaderGL>();
        auto source = vertex->Source();
        auto version = source.source.find("#version");
        auto newline = source.source.find('\n', version);
        if (version == std::string::npos || newline == std::string::npos) {
            diagnostics = "Skinning shader needs a version line";
            return false;
        }
        source.source.insert(newline + 1, "#define ICE_SKINNED\n");
        source.label = "glTF skinned vertex";
        if (!stage->Compile(source, diagnostics))
            return false;
        skinVertex = stage;
    }
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    framebufferSRGB = extensions && (std::strstr(extensions, "GL_EXT_framebuffer_sRGB") ||
                                     std::strstr(extensions, "GL_ARB_framebuffer_sRGB"));
    GLint units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units);
    if (units < 5) {
        diagnostics = "glTF PBR requires five fragment texture units";
        return false;
    }
    if (!composite.Valid()) {
        std::string v, f;
        if (!ShaderText("shaders://project/fullscreen.vert", v, diagnostics) ||
            !ShaderText("shaders://project/display.frag", f, diagnostics) ||
            !composite.Compile(v.c_str(), f.c_str(), diagnostics))
            return false;
    }

    auto next = std::unique_ptr<Resources>(new Resources);
    next->materials = scene.materials;
    if (!icEvaluateScenePose(scene, -1, 0, false, next->rest, diagnostics))
        return false;
    std::set<unsigned> jointNodes;
    std::set<std::pair<unsigned, unsigned>> bones;
    for (auto &skin : scene.skins) {
        std::set<unsigned> members(skin.joints.begin(), skin.joints.end());
        for (auto joint : skin.joints) {
            jointNodes.insert(joint);
            int parent = scene.nodes[joint].parent;
            while (parent >= 0 && !members.count(unsigned(parent)))
                parent = scene.nodes[parent].parent;
            if (parent >= 0)
                bones.insert({joint, unsigned(parent)});
        }
    }
    next->bones.assign(bones.begin(), bones.end());
    next->jointNodes.assign(jointNodes.begin(), jointNodes.end());
    GLint oldActive = 0, oldVBO = 0, oldIBO = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldVBO);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldIBO);
    glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glActiveTexture(GL_TEXTURE0);
    for (auto &primitive : scene.primitives) {
        MeshBuffers mesh;
        mesh.count = (GLsizei)primitive.indices.size();
        mesh.material = primitive.material;
        glGenBuffers(1, &mesh.vertices);
        glGenBuffers(1, &mesh.indices);
        next->meshes.push_back(mesh);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
        glBufferData(GL_ARRAY_BUFFER, primitive.vertices.size() * sizeof(icSceneVertex),
                     primitive.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, primitive.indices.size() * sizeof(std::uint32_t),
                     primitive.indices.data(), GL_STATIC_DRAW);
    }
    // A glTF texture owns sampler state independently even when images are shared.
    for (auto &texture : scene.textures) {
        GLuint name = 0;
        glGenTextures(1, &name);
        next->textures.push_back(name);
        glBindTexture(GL_TEXTURE_2D, name);
        auto &image = scene.images[texture.image];
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture.magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture.minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture.wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture.wrapT);
        bool mip = texture.minFilter != GL_NEAREST && texture.minFilter != GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mip ? GL_TRUE : GL_FALSE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, image.rgba.data());
    }
    // White color/data defaults, then a flat normal map (no global image flipping).
    for (int i = 0; i < 2; i++) {
        GLuint name = 0;
        glGenTextures(1, &name);
        next->textures.push_back(name);
        glBindTexture(GL_TEXTURE_2D, name);
        unsigned char pixel[4] = {255, 255, 255, 255};
        if (i == 1) {
            pixel[0] = 128;
            pixel[1] = 128;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    }
    glBindBuffer(GL_ARRAY_BUFFER, oldVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldIBO);
    glPopClientAttrib();
    glPopAttrib();
    glActiveTexture(oldActive);
    if (glGetError() != GL_NO_ERROR) {
        diagnostics = "GL mesh/texture upload failed; previous scene retained";
        return false;
    }
    for (auto &item : scene.instances) {
        Instance instance;
        instance.world = scene.nodes[item.node].world;
        instance.primitive = item.primitive;
        instance.node = item.node;
        instance.skin = scene.nodes[item.node].skin;
        instance.determinant = determinant(instance.world.data());
        float center[3] = {};
        auto &vertices = scene.primitives[item.primitive].vertices;
        for (auto &v : vertices)
            for (int k = 0; k < 3; k++)
                center[k] += v.position[k] / vertices.size();
        std::copy(center, center + 3, instance.localCenter);
        for (int k = 0; k < 3; k++)
            instance.center[k] = instance.world[k] * center[0] + instance.world[k + 4] * center[1] +
                                 instance.world[k + 8] * center[2] + instance.world[k + 12];
        next->instances.push_back(instance);
    }
    for (auto &m : scene.materials) {
        if (!Pipeline(m, false, false, false, diagnostics))
            return false;
        if (!scene.skins.empty() && !Pipeline(m, false, false, true, diagnostics))
            return false;
    }
    resources = std::move(next);
    return true;
}
bool SceneGL::Draw(const icSceneView &view, std::string &diagnostics) {
    diagnostics.clear();
    if (!resources) {
        diagnostics = "No uploaded scene";
        return false;
    }
    const auto &pose = view.pose ? *view.pose : resources->rest;
    if (pose.world.size() != resources->rest.world.size() ||
        pose.palettes.size() != resources->rest.palettes.size()) {
        diagnostics = "Pose does not match uploaded scene";
        return false;
    }
    for (unsigned i = 0; i < pose.palettes.size(); i++)
        if (pose.palettes[i].size() != resources->rest.palettes[i].size()) {
            diagnostics = "Pose skin palette does not match uploaded scene";
            return false;
        }
    auto instances = resources->instances;
    for (auto &instance : instances) {
        instance.world = pose.world[instance.node];
        instance.determinant = determinant(instance.world.data());
        for (unsigned k = 0; k < 3; k++)
            instance.center[k] = instance.world[k] * instance.localCenter[0] +
                                 instance.world[k + 4] * instance.localCenter[1] +
                                 instance.world[k + 8] * instance.localCenter[2] +
                                 instance.world[k + 12];
        if (instance.skin >= 0) {
            auto &palette = pose.palettes[instance.skin];
            instance.determinant = determinant(palette[0].data());
            for (unsigned k = 0; k < 3; k++) {
                instance.center[k] = 0;
                for (auto &joint : palette)
                    instance.center[k] += joint[k + 12] / palette.size();
            }
        }
    }
    if (view.width <= 0 || view.height <= 0)
        return true;
    if (!target.Resize(view.width, view.height, diagnostics))
        return false;
    GLuint previousFramebuffer = icRenderTargetGL::CurrentFramebuffer();
    GLint previousProgram = 0, matrixMode = 0, active = 0, clientActive = 0, oldVBO = 0, oldIBO = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &clientActive);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldVBO);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &oldIBO);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, view.width, view.height);
    if (framebufferSRGB)
        glDisable(0x8DB9);
    target.Bind();
    glDrawBuffer(0x8CE0);
    glReadBuffer(0x8CE0);
    glDepthMask(GL_TRUE);
    glClearColor(.018f, .025f, .04f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(view.projection.data());
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    std::vector<const Instance *> order;
    for (auto &instance : instances)
        if (std::fabs(instance.determinant) > 1e-12f &&
            (view.material < 0 ||
             (int)resources->meshes[instance.primitive].material == view.material))
            order.push_back(&instance);
    auto depth = [&](const Instance *i) {
        return view.view[2] * i->center[0] + view.view[6] * i->center[1] +
               view.view[10] * i->center[2] + view.view[14];
    };
    std::stable_sort(order.begin(), order.end(), [&](const Instance *a, const Instance *b) {
        bool aa = resources->materials[resources->meshes[a->primitive].material].alphaMode ==
                  icAlphaMode::Blend,
             bb = resources->materials[resources->meshes[b->primitive].material].alphaMode ==
                  icAlphaMode::Blend;
        if (aa != bb)
            return !aa;
        return aa ? depth(a) < depth(b) : false;
    });
    bool ok = true;
    for (auto instance : order) {
        auto &mesh = resources->meshes[instance->primitive];
        auto &material = resources->materials[mesh.material];
        auto pipeline = Pipeline(material, instance->determinant < 0, view.wireframe,
                                 instance->skin >= 0, diagnostics);
        if (!pipeline) {
            ok = false;
            break;
        }
        pipeline->Apply();
        glLoadMatrixf(view.view.data());
        if (instance->skin < 0)
            glMultMatrixf(instance->world.data());
        else {
            auto &palette = pose.palettes[instance->skin];
            glUniformMatrix4fv(pipeline->Uniform("joints[0]"), (GLsizei)palette.size(), GL_FALSE,
                               palette[0].data());
        }
        auto uniform = [&](const char *n) { return pipeline->Uniform(n); };
        glUniform1f(uniform("mirrorSign"), instance->determinant < 0 ? -1.f : 1.f);
        glUniform4f(uniform("baseFactor"), material.baseColor[0], material.baseColor[1],
                    material.baseColor[2], material.baseColor[3]);
        glUniform3f(uniform("emissiveFactor"), material.emissive[0], material.emissive[1],
                    material.emissive[2]);
        glUniform1f(uniform("metallic"), material.metallic);
        glUniform1f(uniform("roughness"), material.roughness);
        glUniform1f(uniform("normalScale"),
                    material.normalMap.texture >= 0 ? material.normalMap.scale : 0);
        glUniform1f(uniform("aoStrength"),
                    material.occlusionMap.texture >= 0 ? material.occlusionMap.scale : 0);
        glUniform1f(uniform("alphaCutoff"), material.alphaCutoff);
        glUniform1f(uniform("exposure"), view.exposure);
        glUniform1i(uniform("alphaMode"), (int)material.alphaMode);
        glUniform1i(uniform("unlit"), material.unlit);
        glUniform1i(uniform("debugMode"), view.debugMode);
        const icSceneTextureSlot slots[] = {material.baseColorMap, material.metalRoughMap,
                                            material.normalMap, material.occlusionMap,
                                            material.emissiveMap};
        const char *samplers[] = {"baseMap", "mrMap", "normalMap", "aoMap", "emissiveMap"};
        const char *uvNames[] = {"baseUV", "mrUV", "normalUV", "aoUV", "emissiveUV"};
        for (int i = 0; i < 5; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            unsigned texture = slots[i].texture >= 0
                                   ? (unsigned)slots[i].texture
                                   : (unsigned)resources->textures.size() - (i == 2 ? 1 : 2);
            glBindTexture(GL_TEXTURE_2D, resources->textures[texture]);
            glUniform1i(uniform(samplers[i]), i);
            glUniform1i(uniform(uvNames[i]), slots[i].uvSet);
        }
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
        GLsizei stride = sizeof(icSceneVertex);
        for (auto &attribute : pipeline->Descriptor().vertices.attributes) {
            const void *offset = (void *)(std::size_t)attribute.offset;
            GLint components = (GLint)attribute.format;
            if (attribute.semantic == icVertexSemantic::Position) {
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(components, GL_FLOAT, stride, offset);
            } else if (attribute.semantic == icVertexSemantic::Normal) {
                glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer(GL_FLOAT, stride, offset);
            } else if (attribute.semantic == icVertexSemantic::Color0) {
                glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(components, GL_FLOAT, stride, offset);
            } else {
                unsigned unit = attribute.semantic == icVertexSemantic::TexCoord0   ? 0
                                : attribute.semantic == icVertexSemantic::TexCoord1 ? 1
                                : attribute.semantic == icVertexSemantic::Tangent   ? 2
                                : attribute.semantic == icVertexSemantic::Joints0   ? 3
                                                                                    : 4;
                glClientActiveTexture(GL_TEXTURE0 + unit);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(components, GL_FLOAT, stride, offset);
            }
        }
        glDrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
    }
    icRenderTargetGL::BindFramebuffer(previousFramebuffer);
    glDrawBuffer(previousFramebuffer ? 0x8CE0 : GL_BACK);
    glReadBuffer(previousFramebuffer ? 0x8CE0 : GL_BACK);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, target.Color());
    composite.Bind();
    glUniform1i(composite.Uniform("image"), 0);
    glUniform1i(composite.Uniform("debugMode"), view.debugMode);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-1, -1);
    glTexCoord2f(1, 0);
    glVertex2f(1, -1);
    glTexCoord2f(1, 1);
    glVertex2f(1, 1);
    glTexCoord2f(0, 1);
    glVertex2f(-1, 1);
    glEnd();
    if (view.skeleton) {
        glUseProgram(0);
        for (unsigned unit = 0; unit < 5; unit++) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glDisable(GL_TEXTURE_2D);
        }
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(view.projection.data());
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(view.view.data());
        glColor3f(1, .7f, .15f);
        glLineWidth(2);
        glBegin(GL_LINES);
        for (auto edge : resources->bones) {
            glVertex3fv(pose.world[edge.first].data() + 12);
            glVertex3fv(pose.world[edge.second].data() + 12);
        }
        glEnd();
        glPointSize(4);
        glBegin(GL_POINTS);
        for (auto joint : resources->jointNodes)
            glVertex3fv(pose.world[joint].data() + 12);
        glEnd();
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(matrixMode);
    glUseProgram(previousProgram);
    glPopClientAttrib();
    glPopAttrib();
    glBindBuffer(GL_ARRAY_BUFFER, oldVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oldIBO);
    glClientActiveTexture(clientActive);
    glActiveTexture(active);
    if (glGetError() != GL_NO_ERROR) {
        diagnostics = "GL scene draw/state restoration failed";
        return false;
    }
    return ok;
}
} // namespace
std::unique_ptr<icSceneRenderer>
icCreateSceneRendererGL(std::shared_ptr<icResourceResolver> resolver) {
    return std::unique_ptr<icSceneRenderer>(new SceneGL(std::move(resolver)));
}
