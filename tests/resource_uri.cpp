#include "ice/io/resource_uri.h"
#include "ice/assets/scene_asset.h"
#include <fstream>
#include <iostream>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::cerr << "URI failed " << __LINE__ << ": " << #x << " (" << error << ")\n";        \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
class Memory final : public ice::io::virtual_scheme_handler {
public:
	std::map<std::string, std::vector<unsigned char>> data;
	ice::io::resource_read_result                     read(const ice::io::resource_uri &uri,
	                                                       std::vector<unsigned char>  &bytes,
	                                                       std::string                 &error) override {
        auto entry = data.find(uri.document());
        if (entry == data.end()) {
            error = "Missing memory resource " + uri.string();
            return ice::io::resource_read_result::not_found;
        }
        if (uri.path.find("broken") != std::string::npos) {
            error = "Provider failure";
            return ice::io::resource_read_result::error;
        }
        bytes = entry->second;
        return ice::io::resource_read_result::success;
	}
};
int main(int argc, char **argv) {
	if (argc != 2)
		return 2;
	std::string           error;
	ice::io::resource_uri base, resolved;
	CHECK(ice::io::resource_uri::parse("ASSETS://project/dir/model.gltf?rev=1#old", base, error));
	CHECK(ice::io::resource_uri::resolve(base, "../tex/a%20b.png#child", resolved, error));
	CHECK(resolved.string() == "assets://project/tex/a%20b.png#child");
	CHECK(ice::io::resource_uri::resolve(base, "#next", resolved, error));
	CHECK(resolved.string() == "assets://project/dir/model.gltf?rev=1#next");
	CHECK(ice::io::resource_uri::resolve(base, "?rev=2", resolved, error));
	CHECK(resolved.string() == "assets://project/dir/model.gltf?rev=2");
	CHECK(ice::io::resource_uri::parse("textures://project/a//b/%7e/%2f", resolved, error));
	CHECK(resolved.string() == "textures://project/a//b/~/%2F");
	CHECK(ice::io::resource_uri::parse(
	    "script://project/table.lua?seed=42&tag=wood&tag=metal&label=a%2Bb#prepared", resolved,
	    error));
	CHECK(resolved.query == "seed=42&tag=wood&tag=metal&label=a%2Bb" && resolved.has_fragment &&
	      resolved.fragment == "prepared");
	CHECK(!ice::io::resource_uri::parse("assets://project/%Q0", resolved, error));
	ice::io::resource_resolver resolver;
	auto                       memory = std::make_shared<Memory>();
	resolver.register_handler("memory", memory);
	ice::io::resource_uri root;
	CHECK(ice::io::resource_uri::from_path(argv[1], root, error));
	std::string directory = root.string();
	if (directory.back() != '/')
		directory += '/';
	CHECK(resolver.mount("assets://project/", directory, error));
	ice::assets::scene_asset local;
	CHECK(ice::assets::load_gltf("assets://project/Interleaved.gltf", resolver, local, error));
	CHECK(local.source == "assets://project/Interleaved.gltf");
	CHECK(local.dependencies.size() == 2 &&
	      local.dependencies[1] == "assets://project/mesh%20data.bin");
	std::vector<unsigned char> file, buffer;
	CHECK(resolver.read(directory + "Interleaved.gltf", file, error));
	CHECK(resolver.read(directory + "mesh%20data.bin", buffer, error));
	memory->data["memory://pack/Interleaved.gltf"] = file;
	memory->data["memory://pack/mesh%20data.bin"]  = buffer;
	CHECK(resolver.mount("assets://project/", "memory://pack/", error));
	ice::assets::scene_asset remote;
	CHECK(ice::assets::load_gltf("assets://project/Interleaved.gltf", resolver, remote, error));
	CHECK(remote.source == local.source && remote.bounds_max[0] == local.bounds_max[0]);
	// URI escaping is decoded only by the final provider, never twice by glTF.
	std::string json(file.begin(), file.end());
	auto        p = json.find("mesh%20data.bin");
	CHECK(p != std::string::npos);
	json.replace(p, 15, "mesh%2520data.bin");
	memory->data["memory://pack/literal.gltf"]      = {json.begin(), json.end()};
	memory->data["memory://pack/mesh%2520data.bin"] = buffer;
	CHECK(ice::assets::load_gltf("assets://project/literal.gltf", resolver, remote, error));
	CHECK(resolver.mount("textures://project/", "assets://project/", error));
	CHECK(resolver.read("textures://project/mesh%20data.bin#object", file, error) &&
	      file == buffer);
	memory->data["memory://pack/version?rev=2"] = {'v', '2'};
	CHECK(resolver.read("assets://project/version?rev=2", file, error) && file.size() == 2);
	std::string project                              = "[mounts]\nmodels://project/ = ../\n";
	memory->data["memory://pack/config/project.ice"] = {project.begin(), project.end()};
	CHECK(resolver.load_project("memory://pack/config/project.ice", error));
	CHECK(resolver.read("models://project/mesh%20data.bin", file, error));
	std::string bad                       = "models://project/ = memory://else/\nbad line";
	memory->data["memory://pack/bad.ice"] = {bad.begin(), bad.end()};
	CHECK(!resolver.load_project("memory://pack/bad.ice", error));
	CHECK(resolver.read("models://project/mesh%20data.bin", file, error));
	CHECK(resolver.mount("cycle://a/", "cycle://b/", error) &&
	      resolver.mount("cycle://b/", "cycle://a/", error));
	auto old = file;
	CHECK(!resolver.read("cycle://a/x", file, error) && file == old);
	CHECK(!resolver.read("https://example.invalid/a", file, error));
	memory->data["memory://game/shared.frag"]              = {'g'};
	memory->data["memory://engine/shared.frag"]            = {'e'};
	memory->data["memory://engine/default.frag?variant=2"] = {'d'};
	CHECK(
	    resolver.mount_search("shaders://project/", {"memory://game/", "memory://engine/"}, error));
	CHECK(resolver.read("shaders://project/shared.frag", file, error) &&
	      file == std::vector<unsigned char>{'g'});
	CHECK(resolver.read("shaders://project/default.frag?variant=2#stage", file, error) &&
	      file == std::vector<unsigned char>{'d'});
	old = file;
	CHECK(!resolver.read("shaders://project/missing.frag", file, error) && file == old);
	CHECK(error.find("memory://game/missing.frag") != std::string::npos &&
	      error.find("memory://engine/missing.frag") != std::string::npos);
	memory->data["memory://game/broken"]   = {'x'};
	memory->data["memory://engine/broken"] = {'e'};
	CHECK(!resolver.read("shaders://project/broken", file, error) && error == "Provider failure" &&
	      file == old);
	// Empty files exist and override engine content; compiler errors must not trigger fallback.
	memory->data["memory://game/shared.frag"] = {};
	CHECK(resolver.read("shaders://project/shared.frag", file, error) && file.empty());
	project =
	    "[search]\nshaders://project/ = memory://engine/\nshaders://project/ = memory://game/\n";
	memory->data["memory://pack/search.ice"] = {project.begin(), project.end()};
	CHECK(resolver.load_project("memory://pack/search.ice", error));
	CHECK(resolver.read("shaders://project/shared.frag", file, error) &&
	      file == std::vector<unsigned char>{'e'});
	std::cout << "URI passed: resolve/escape/query/fragment, project mounts, custom provider, glTF "
	             "dependencies, provider switch, cycle and transactional failures\n";
}
