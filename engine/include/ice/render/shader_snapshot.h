#pragma once
#include "ice/core/expected.h"
#include "ice/io/resource_uri.h"
#include "ice/render/types.h"
#include <string>
#include <string_view>

namespace ice::render {
/// Immutable source read once through a URI provider. Owns nonempty UTF-8 without NUL,
/// at most 1 MiB, its canonical requested URI and SHA-256 of the exact retained bytes.
/// Providers remain responsible for their own read/allocation budgets. This is source
/// provenance, not a signature, dependency manifest or sandbox for a native shader compiler.
class shader_snapshot {
public:
	/// Resolve and read once. Failure publishes no snapshot; no GPU or network is implied.
	static ice::expected<shader_snapshot, diagnostic>
	read(const ice::io::resource_resolver &resolver, const std::string &uri);
	/// Canonical requested URI. The resolver reads its document/query; a fragment is retained
	/// only as caller identity, matching the resolver's existing fragment-selector semantics.
	const std::string &uri() const { return uri_; }
	/// Exact validated source bytes; immutable for this owner's lifetime.
	std::string_view source() const { return source_; }
	/// Lowercase SHA-256 hex digest of source(), independent of URI or backend entry names.
	const std::string &sha256() const { return sha256_; }

private:
	shader_snapshot(std::string uri, std::string source, std::string sha256);
	std::string uri_, source_, sha256_;
};
} // namespace ice::render
