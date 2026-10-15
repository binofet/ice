#include "ice/io/resource_uri.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <set>
#include <sstream>
namespace ice::io {
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
} // namespace ice::io
#include <windows.h>
namespace ice::io {
#else
} // namespace ice::io
#include <unistd.h>
namespace ice::io {
#endif
namespace {
std::string lower(std::string s) {
	for (char &c : s)
		c = (char)std::tolower((unsigned char)c);
	return s;
}
int hex(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}
bool unreserved(unsigned char c) {
	return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
}
bool escape_normalize(const std::string &in, std::string &out, std::string &error) {
	const char *digits = "0123456789ABCDEF";
	out.clear();
	for (std::size_t i = 0; i < in.size(); i++) {
		unsigned char c = in[i];
		if (c == '%') {
			if (i + 2 >= in.size() || hex(in[i + 1]) < 0 || hex(in[i + 2]) < 0) {
				error = "Invalid URI percent escape";
				return false;
			}
			c = (unsigned char)(hex(in[i + 1]) * 16 + hex(in[i + 2]));
			if (!c) {
				error = "NUL is not allowed in a URI";
				return false;
			}
			if (unreserved(c))
				out += (char)c;
			else {
				out += '%';
				out += digits[c >> 4];
				out += digits[c & 15];
			}
			i += 2;
		} else {
			if (c <= 32 || c == 127 || c == '\\') {
				error = "Escape whitespace/control characters and use URI slash separators";
				return false;
			}
			out += (char)c;
		}
	}
	return true;
}
std::string dots(std::string input) {
	std::string out;
	while (!input.empty()) {
		if (input.compare(0, 3, "../") == 0)
			input.erase(0, 3);
		else if (input.compare(0, 2, "./") == 0)
			input.erase(0, 2);
		else if (input.compare(0, 3, "/./") == 0)
			input.erase(0, 2);
		else if (input == "/.")
			input = "/";
		else if (input.compare(0, 4, "/../") == 0 || input == "/..") {
			input      = input == "/.." ? "/" : input.substr(3);
			auto slash = out.find_last_of('/');
			out        = slash == std::string::npos ? "" : out.substr(0, slash);
		} else if (input == "." || input == "..")
			input.clear();
		else {
			auto end = input.find('/', input[0] == '/' ? 1 : 0);
			if (end == std::string::npos) {
				out += input;
				input.clear();
			} else {
				out += input.substr(0, end);
				input.erase(0, end);
			}
		}
	}
	return out;
}

std::string trim(const std::string &s) {
	auto a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
class file_handler final : public virtual_scheme_handler {
public:
	resource_read_result read(const resource_uri &uri, std::vector<unsigned char> &out,
	                          std::string &error) override {
		if ((!uri.authority.empty() && lower(uri.authority) != "localhost") || uri.has_query) {
			error = "file URI requires a local host and no query";
			return resource_read_result::error;
		}
		if (uri.path.empty() || uri.path[0] != '/') {
			error = "file URI path must be absolute";
			return resource_read_result::error;
		}
		std::string path;
		for (std::size_t i = 0; i < uri.path.size(); i++) {
			char c = uri.path[i];
			if (c == '%') {
				c = (char)(hex(uri.path[i + 1]) * 16 + hex(uri.path[i + 2]));
				i += 2;
				if (c == '/' || c == '\\') {
					error = "Encoded file separators are not supported";
					return resource_read_result::error;
				}
			}
			path += c;
		}
#ifdef _WIN32
		if (path.size() > 2 && path[0] == '/' && path[2] == ':')
			path.erase(0, 1);
#endif
		errno = 0;
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) {
			int failure = errno;
			error       = "Cannot open " + uri.document();
			return failure == ENOENT || failure == ENOTDIR ? resource_read_result::not_found
			                                               : resource_read_result::error;
		}
		auto length = file.tellg();
		if (length < 0 || length > 256ll * 1024 * 1024) {
			error = "Resource exceeds 256 MiB read limit";
			return resource_read_result::error;
		}
		std::vector<unsigned char> next((std::size_t)length);
		file.seekg(0);
		if (length && !file.read((char *)next.data(), length)) {
			error = "Cannot read " + uri.document();
			return resource_read_result::error;
		}
		out = std::move(next);
		return resource_read_result::success;
	}
};
} // namespace
std::string resource_uri::string() const {
	return (scheme.empty() ? "" : scheme + ":") + (has_authority ? "//" + authority : "") + path +
	       (has_query ? "?" + query : "") + (has_fragment ? "#" + fragment : "");
}
std::string resource_uri::document() const {
	auto copy         = *this;
	copy.has_fragment = false;
	copy.fragment.clear();
	return copy.string();
}
bool resource_uri::parse(const std::string &uri_text, resource_uri &output,
                         std::string &diagnostics) {
	diagnostics.clear();
	resource_uri uri;
	std::string  text = uri_text;
	auto         hash = text.find('#');
	if (hash != std::string::npos) {
		uri.has_fragment = true;
		if (!escape_normalize(text.substr(hash + 1), uri.fragment, diagnostics))
			return false;
		text.resize(hash);
	}
	auto question = text.find('?');
	if (question != std::string::npos) {
		uri.has_query = true;
		if (!escape_normalize(text.substr(question + 1), uri.query, diagnostics))
			return false;
		text.resize(question);
	}
	auto colon = text.find(':'), slash = text.find('/');
	if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
		uri.scheme = lower(text.substr(0, colon));
		if (uri.scheme.empty() || !std::isalpha((unsigned char)uri.scheme[0])) {
			diagnostics = "Invalid URI scheme";
			return false;
		}
		for (char c : uri.scheme)
			if (!std::isalnum((unsigned char)c) && c != '+' && c != '-' && c != '.') {
				diagnostics = "Invalid URI scheme";
				return false;
			}
		text.erase(0, colon + 1);
	}
	if (text.compare(0, 2, "//") == 0) {
		uri.has_authority     = true;
		auto        end       = text.find('/', 2);
		std::string authority = text.substr(2, end == std::string::npos ? end : end - 2);
		if (!escape_normalize(authority, uri.authority, diagnostics))
			return false;
		text = end == std::string::npos ? "" : text.substr(end);
	}
	if (!escape_normalize(text, uri.path, diagnostics))
		return false;
	if (uri.has_authority || (!uri.path.empty() && uri.path[0] == '/'))
		uri.path = dots(uri.path);
	output = std::move(uri);
	return true;
}
bool resource_uri::resolve(const resource_uri &base, const std::string &reference,
                           resource_uri &output, std::string &diagnostics) {
	resource_uri ref;
	if (!parse(reference, ref, diagnostics))
		return false;
	if (!ref.scheme.empty()) {
		output = ref;
		return true;
	}
	auto result   = ref;
	result.scheme = base.scheme;
	if (!ref.has_authority) {
		result.has_authority = base.has_authority;
		result.authority     = base.authority;
		if (ref.path.empty()) {
			result.path = base.path;
			if (!ref.has_query) {
				result.has_query = base.has_query;
				result.query     = base.query;
			}
		} else if (ref.path[0] != '/') {
			auto slash  = base.path.find_last_of('/');
			result.path = dots((slash == std::string::npos ? (base.has_authority ? "/" : "")
			                                               : base.path.substr(0, slash + 1)) +
			                   ref.path);
		}
	}
	output = std::move(result);
	return true;
}
bool resource_uri::from_path(const std::string &native_path, resource_uri &output,
                             std::string &diagnostics) {
	std::string path = native_path;
#ifdef _WIN32
	char absolute[4096];
	if (!GetFullPathNameA(path.c_str(), sizeof(absolute), absolute, nullptr)) {
		diagnostics = "Cannot resolve native path";
		return false;
	}
	path = absolute;
	std::replace(path.begin(), path.end(), '\\', '/');
	if (path.empty() || path[0] != '/')
		path = "/" + path;
#else
	if (path.empty() || path[0] != '/') {
		char cwd[4096];
		if (!getcwd(cwd, sizeof(cwd))) {
			diagnostics = "Cannot resolve current directory";
			return false;
		}
		path = std::string(cwd) + "/" + path;
	}
#endif
	const char *digits = "0123456789ABCDEF";
	std::string encoded;
	for (unsigned char c : path) {
		if (unreserved(c) || c == '/' || c == ':')
			encoded += (char)c;
		else {
			encoded += '%';
			encoded += digits[c >> 4];
			encoded += digits[c & 15];
		}
	}
	return parse("file://" + encoded, output, diagnostics);
}
resource_resolver::resource_resolver() {
	register_handler("file", std::make_shared<file_handler>());
}
void resource_resolver::register_handler(const std::string                      &scheme,
                                         std::shared_ptr<virtual_scheme_handler> handler) {
	handlers_[lower(scheme)] = std::move(handler);
}
bool resource_resolver::canonical(const std::string &uri_or_path, resource_uri &output,
                                  std::string &diagnostics) const {
	auto colon = uri_or_path.find(':');
	bool native =
	    colon == std::string::npos || (colon == 1 && std::isalpha((unsigned char)uri_or_path[0]));
	return native ? resource_uri::from_path(uri_or_path, output, diagnostics)
	              : resource_uri::parse(uri_or_path, output, diagnostics);
}
bool resource_resolver::mount(const std::string &virtual_root, const std::string &target_root,
                              std::string &diagnostics) {
	return mount_search(virtual_root, {target_root}, diagnostics);
}
bool resource_resolver::mount_search(const std::string              &virtual_root,
                                     const std::vector<std::string> &target_roots,
                                     std::string                    &diagnostics) {
	resource_uri a;
	if (!resource_uri::parse(virtual_root, a, diagnostics))
		return false;
	auto root = [](const resource_uri &u) {
		return !u.scheme.empty() && !u.path.empty() && u.path.back() == '/' && !u.has_query &&
		       !u.has_fragment;
	};
	if (!root(a) || target_roots.empty()) {
		diagnostics = "Mounts require absolute URI directory roots and at least one target";
		return false;
	}
	std::vector<std::string> next;
	for (auto &target : target_roots) {
		resource_uri b;
		if (!resource_uri::parse(target, b, diagnostics))
			return false;
		if (!root(b)) {
			diagnostics =
			    "Mount targets require absolute URI directory roots without query/fragment";
			return false;
		}
		next.push_back(b.string());
	}
	mounts_[a.string()] = std::move(next);
	return true;
}
bool resource_resolver::read(const std::string &uri_text, std::vector<unsigned char> &bytes,
                             std::string &diagnostics) const {
	diagnostics.clear();
	resource_uri uri;
	if (!canonical(uri_text, uri, diagnostics))
		return false;
	uri.has_fragment = false;
	uri.fragment.clear();
	std::set<std::string> active;
	return read_resolved(uri, bytes, diagnostics, active, 0) == resource_read_result::success;
}
resource_read_result resource_resolver::read_resolved(const resource_uri         &uri,
                                                      std::vector<unsigned char> &bytes,
                                                      std::string                &error,
                                                      std::set<std::string>      &active,
                                                      unsigned                    depth) const {
	if (depth >= 32) {
		error = "URI mount chain exceeds 32 entries";
		return resource_read_result::error;
	}
	auto key = uri.document();
	if (!active.insert(key).second) {
		error = "URI mount cycle: " + key;
		return resource_read_result::error;
	}
	struct pop {
		std::set<std::string> &set;
		std::string            key;
		~pop() { set.erase(key); }
	} pop{active, key};
	const std::pair<const std::string, std::vector<std::string>> *match = nullptr;
	for (auto &mount : mounts_)
		if (key.compare(0, mount.first.size(), mount.first) == 0 &&
		    (!match || mount.first.size() > match->first.size()))
			match = &mount;
	if (match) {
		std::string searched = "Resource not found: " + key;
		for (auto &target : match->second) {
			resource_uri candidate;
			if (!resource_uri::parse(target + key.substr(match->first.size()), candidate, error))
				return resource_read_result::error;
			std::string reason;
			auto        result = read_resolved(candidate, bytes, reason, active, depth + 1);
			if (result == resource_read_result::success) {
				error.clear();
				return result;
			}
			if (result == resource_read_result::error) {
				error = reason;
				return result;
			}
			searched += "\n  " + reason;
		}
		error = searched;
		return resource_read_result::not_found;
	}
	auto handler = handlers_.find(uri.scheme);
	if (handler == handlers_.end() || !handler->second) {
		error = "No URI scheme handler for " + uri.scheme;
		return resource_read_result::error;
	}
	std::vector<unsigned char> next;
	auto                       result = handler->second->read(uri, next, error);
	if (result == resource_read_result::success) {
		bytes = std::move(next);
		error.clear();
	}
	return result;
}
bool resource_resolver::load_project(const std::string &project_uri_or_path,
                                     std::string       &diagnostics) {
	resource_uri base;
	if (!canonical(project_uri_or_path, base, diagnostics))
		return false;
	std::vector<unsigned char> bytes;
	if (!read(base.string(), bytes, diagnostics))
		return false;
	resource_resolver                               next = *this;
	std::istringstream                              input(std::string(bytes.begin(), bytes.end()));
	std::string                                     line;
	unsigned                                        number = 0;
	bool                                            search = false;
	std::map<std::string, std::vector<std::string>> searches;
	while (std::getline(input, line)) {
		++number;
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line == "[mounts]") {
			search = false;
			continue;
		}
		if (line == "[search]") {
			search = true;
			continue;
		}
		auto eq = line.find('=');
		if (eq == std::string::npos) {
			diagnostics = "Invalid project mount at line " + std::to_string(number);
			return false;
		}
		auto         a = trim(line.substr(0, eq)), b = trim(line.substr(eq + 1));
		resource_uri target;
		if (!resource_uri::resolve(base, b, target, diagnostics))
			return false;
		if (search)
			searches[a].push_back(target.string());
		else if (!next.mount(a, target.string(), diagnostics))
			return false;
	}
	for (auto &entry : searches)
		if (!next.mount_search(entry.first, entry.second, diagnostics))
			return false;
	mounts_ = std::move(next.mounts_);
	return true;
}

} // namespace ice::io
