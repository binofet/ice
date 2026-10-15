#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
namespace ice::io {

/// Hierarchical resource URI. Parsing normalizes scheme/dot segments and preserves
/// escaped reserved characters. Native paths enter only through FromPath().
struct resource_uri {
	/// URI fragment or retained fragment stage, as defined by the record. URI query text, retained
	/// separately from the path. URI path or animated property, as determined by the enclosing
	/// record type. URI authority text, with reserved escaping preserved. Normalized URI scheme
	/// selecting a registered virtual handler.
	std::string scheme, authority, path, query, fragment;
	/// Whether a fragment delimiter was present, including an empty fragment. Whether a query
	/// delimiter was present, including an empty query. Whether the URI explicitly contains an
	/// authority component.
	bool has_authority = false, has_query = false, has_fragment = false;
	/// Return the normalized textual URI including explicitly present query/fragment delimiters.
	std::string string() const;
	/// Return the resource document URI without a fragment selector.
	std::string document() const;
	/// Parse and normalize a hierarchical URI. False records diagnostics and preserves the output
	/// candidate.
	static bool parse(const std::string &uri_text, resource_uri &output, std::string &diagnostics);
	/// Resolve a reference against a base URI, preserving escaped reserved characters; failure is
	/// transactional.
	static bool resolve(const resource_uri &base, const std::string &reference,
	                    resource_uri &output, std::string &diagnostics);
	/// Convert a native filesystem path to a file URI; failure records diagnostics.
	static bool from_path(const std::string &native_path, resource_uri &output,
	                      std::string &diagnostics);
};
/// Handler result: only not_found permits searching another mount; error stops resolution.
enum class resource_read_result { success, not_found, error };
/// Custom URI scheme reader. The resolver retains shared ownership; implementations report missing
/// resources separately from read failures.
class virtual_scheme_handler {
public:
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~virtual_scheme_handler() {}
	/// Read into caller-owned output storage; inspect the returned count/status and diagnostics
	/// before consuming data.
	virtual resource_read_result read(const resource_uri &uri, std::vector<unsigned char> &bytes,
	                                  std::string &diagnostics) = 0;
};
/// URI handler registry and ordered mount search paths. Configure before concurrent reads; handler
/// thread safety remains the handler responsibility.
class resource_resolver {
public:
	/// Register file:// access only; no implicit network transport or HTTP requests are enabled.
	resource_resolver(); // registers file://; no network access or implicit HTTP requests
	/// Retain a scheme handler in the resolver; replacing a scheme changes subsequent reads only.
	void register_handler(const std::string                      &scheme,
	                      std::shared_ptr<virtual_scheme_handler> handler);
	/// Register a virtual URI root mapped to a target URI root; false rejects an invalid mapping.
	bool mount(const std::string &virtual_root, const std::string &target_root,
	           std::string &diagnostics);
	/// First existing resource wins. Only NotFound permits trying the next root.
	bool mount_search(const std::string &virtual_root, const std::vector<std::string> &target_roots,
	                  std::string &diagnostics);
	/// Read project URI mount/search configuration; false records a validation/read diagnostic.
	bool load_project(const std::string &project_uri_or_path, std::string &diagnostics);
	/// Read into caller-owned output storage; inspect the returned count/status and diagnostics
	/// before consuming data.
	bool read(const std::string &uri_text, std::vector<unsigned char> &bytes,
	          std::string &diagnostics) const;
	/// Resolve a URI/native path to normalized URI components; false records diagnostics.
	bool canonical(const std::string &uri_or_path, resource_uri &output,
	               std::string &diagnostics) const;

private:
	std::map<std::string, std::shared_ptr<virtual_scheme_handler>> handlers_;
	std::map<std::string, std::vector<std::string>>                mounts_;
	resource_read_result read_resolved(const resource_uri &uri, std::vector<unsigned char> &bytes,
	                                   std::string &error, std::set<std::string> &active,
	                                   unsigned depth) const;
};

} // namespace ice::io
