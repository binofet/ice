#include "ice/render/shader_snapshot.h"
#include "ice/core/sha256.h"
#include "ice/core/utf8.h"
#include <utility>
#include <vector>

namespace ice::render {
shader_snapshot::shader_snapshot(std::string uri, std::string source, std::string sha256)
    : uri_(std::move(uri)), source_(std::move(source)), sha256_(std::move(sha256)) {}
ice::expected<shader_snapshot, diagnostic>
shader_snapshot::read(const ice::io::resource_resolver &resolver, const std::string &uri) {
	ice::io::resource_uri canonical;
	std::string           error;
	if (!ice::core::valid_utf8_text(uri, 4096) || uri.empty() ||
	    !resolver.canonical(uri, canonical, error))
		return ice::unexpected(diagnostic{"Invalid shader URI: " + error});
	auto name = canonical.string();
	if (name.size() > 4096)
		return ice::unexpected(diagnostic{"Canonical shader URI exceeds 4096 bytes"});
	std::vector<unsigned char> bytes;
	if (!resolver.read(name, bytes, error))
		return ice::unexpected(diagnostic{name + ": " + error});
	if (bytes.empty() || bytes.size() > 1024 * 1024)
		return ice::unexpected(diagnostic{name + ": shader source must contain 1..1048576 bytes"});
	std::string source(bytes.begin(), bytes.end());
	if (!ice::core::valid_utf8_text(source))
		return ice::unexpected(diagnostic{name + ": shader source must be UTF-8 without NUL"});
	auto hash = ice::core::sha256(source);
	return shader_snapshot(std::move(name), std::move(source), std::move(hash));
}
} // namespace ice::render
