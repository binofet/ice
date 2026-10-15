#include "Content/icResourceURI.h"
#include "Content/icSceneAsset.h"
#include <fstream>
#include <iostream>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::cerr << "URI failed " << __LINE__ << ": " << #x << " (" << error << ")\n";        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
class Memory final : public icVirtualSchemeHandler {
  public:
    std::map<std::string, std::vector<unsigned char>> data;
    icResourceReadResult Read(const icResourceURI &uri, std::vector<unsigned char> &bytes,
                              std::string &error) override {
        auto entry = data.find(uri.Document());
        if (entry == data.end()) {
            error = "Missing memory resource " + uri.String();
            return icResourceReadResult::NotFound;
        }
        if (uri.path.find("broken") != std::string::npos) {
            error = "Provider failure";
            return icResourceReadResult::Error;
        }
        bytes = entry->second;
        return icResourceReadResult::Success;
    }
};
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::string error;
    icResourceURI base, resolved;
    CHECK(icResourceURI::Parse("ASSETS://project/dir/model.gltf?rev=1#old", base, error));
    CHECK(icResourceURI::Resolve(base, "../tex/a%20b.png#child", resolved, error));
    CHECK(resolved.String() == "assets://project/tex/a%20b.png#child");
    CHECK(icResourceURI::Resolve(base, "#next", resolved, error));
    CHECK(resolved.String() == "assets://project/dir/model.gltf?rev=1#next");
    CHECK(icResourceURI::Resolve(base, "?rev=2", resolved, error));
    CHECK(resolved.String() == "assets://project/dir/model.gltf?rev=2");
    CHECK(icResourceURI::Parse("textures://project/a//b/%7e/%2f", resolved, error));
    CHECK(resolved.String() == "textures://project/a//b/~/%2F");
    CHECK(icResourceURI::Parse(
        "script://project/table.lua?seed=42&tag=wood&tag=metal&label=a%2Bb#prepared", resolved,
        error));
    CHECK(resolved.query == "seed=42&tag=wood&tag=metal&label=a%2Bb" && resolved.hasFragment &&
          resolved.fragment == "prepared");
    CHECK(!icResourceURI::Parse("assets://project/%Q0", resolved, error));
    icResourceResolver resolver;
    auto memory = std::make_shared<Memory>();
    resolver.Register("memory", memory);
    icResourceURI root;
    CHECK(icResourceURI::FromPath(argv[1], root, error));
    std::string directory = root.String();
    if (directory.back() != '/')
        directory += '/';
    CHECK(resolver.Mount("assets://project/", directory, error));
    icSceneAsset local;
    CHECK(icLoadGLTF("assets://project/Interleaved.gltf", resolver, local, error));
    CHECK(local.source == "assets://project/Interleaved.gltf");
    CHECK(local.dependencies.size() == 2 &&
          local.dependencies[1] == "assets://project/mesh%20data.bin");
    std::vector<unsigned char> file, buffer;
    CHECK(resolver.Read(directory + "Interleaved.gltf", file, error));
    CHECK(resolver.Read(directory + "mesh%20data.bin", buffer, error));
    memory->data["memory://pack/Interleaved.gltf"] = file;
    memory->data["memory://pack/mesh%20data.bin"] = buffer;
    CHECK(resolver.Mount("assets://project/", "memory://pack/", error));
    icSceneAsset remote;
    CHECK(icLoadGLTF("assets://project/Interleaved.gltf", resolver, remote, error));
    CHECK(remote.source == local.source && remote.boundsMax[0] == local.boundsMax[0]);
    // URI escaping is decoded only by the final provider, never twice by glTF.
    std::string json(file.begin(), file.end());
    auto p = json.find("mesh%20data.bin");
    CHECK(p != std::string::npos);
    json.replace(p, 15, "mesh%2520data.bin");
    memory->data["memory://pack/literal.gltf"] = {json.begin(), json.end()};
    memory->data["memory://pack/mesh%2520data.bin"] = buffer;
    CHECK(icLoadGLTF("assets://project/literal.gltf", resolver, remote, error));
    CHECK(resolver.Mount("textures://project/", "assets://project/", error));
    CHECK(resolver.Read("textures://project/mesh%20data.bin#object", file, error) &&
          file == buffer);
    memory->data["memory://pack/version?rev=2"] = {'v', '2'};
    CHECK(resolver.Read("assets://project/version?rev=2", file, error) && file.size() == 2);
    std::string project = "[mounts]\nmodels://project/ = ../\n";
    memory->data["memory://pack/config/project.ice"] = {project.begin(), project.end()};
    CHECK(resolver.LoadProject("memory://pack/config/project.ice", error));
    CHECK(resolver.Read("models://project/mesh%20data.bin", file, error));
    std::string bad = "models://project/ = memory://else/\nbad line";
    memory->data["memory://pack/bad.ice"] = {bad.begin(), bad.end()};
    CHECK(!resolver.LoadProject("memory://pack/bad.ice", error));
    CHECK(resolver.Read("models://project/mesh%20data.bin", file, error));
    CHECK(resolver.Mount("cycle://a/", "cycle://b/", error) &&
          resolver.Mount("cycle://b/", "cycle://a/", error));
    auto old = file;
    CHECK(!resolver.Read("cycle://a/x", file, error) && file == old);
    CHECK(!resolver.Read("https://example.invalid/a", file, error));
    memory->data["memory://game/shared.frag"] = {'g'};
    memory->data["memory://engine/shared.frag"] = {'e'};
    memory->data["memory://engine/default.frag?variant=2"] = {'d'};
    CHECK(
        resolver.MountSearch("shaders://project/", {"memory://game/", "memory://engine/"}, error));
    CHECK(resolver.Read("shaders://project/shared.frag", file, error) &&
          file == std::vector<unsigned char>{'g'});
    CHECK(resolver.Read("shaders://project/default.frag?variant=2#stage", file, error) &&
          file == std::vector<unsigned char>{'d'});
    old = file;
    CHECK(!resolver.Read("shaders://project/missing.frag", file, error) && file == old);
    CHECK(error.find("memory://game/missing.frag") != std::string::npos &&
          error.find("memory://engine/missing.frag") != std::string::npos);
    memory->data["memory://game/broken"] = {'x'};
    memory->data["memory://engine/broken"] = {'e'};
    CHECK(!resolver.Read("shaders://project/broken", file, error) && error == "Provider failure" &&
          file == old);
    // Empty files exist and override engine content; compiler errors must not trigger fallback.
    memory->data["memory://game/shared.frag"] = {};
    CHECK(resolver.Read("shaders://project/shared.frag", file, error) && file.empty());
    project =
        "[search]\nshaders://project/ = memory://engine/\nshaders://project/ = memory://game/\n";
    memory->data["memory://pack/search.ice"] = {project.begin(), project.end()};
    CHECK(resolver.LoadProject("memory://pack/search.ice", error));
    CHECK(resolver.Read("shaders://project/shared.frag", file, error) &&
          file == std::vector<unsigned char>{'e'});
    std::cout << "URI passed: resolve/escape/query/fragment, project mounts, custom provider, glTF "
                 "dependencies, provider switch, cycle and transactional failures\n";
}
