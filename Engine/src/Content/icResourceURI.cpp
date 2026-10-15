#include "Content/icResourceURI.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <fstream>
#include <set>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
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
bool escapeNormalize(const std::string &in, std::string &out, std::string &error) {
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
            input = input == "/.." ? "/" : input.substr(3);
            auto slash = out.find_last_of('/');
            out = slash == std::string::npos ? "" : out.substr(0, slash);
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
class FileHandler final : public icVirtualSchemeHandler {
  public:
    icResourceReadResult Read(const icResourceURI &uri, std::vector<unsigned char> &out,
                              std::string &error) override {
        if ((!uri.authority.empty() && lower(uri.authority) != "localhost") || uri.hasQuery) {
            error = "file URI requires a local host and no query";
            return icResourceReadResult::Error;
        }
        if (uri.path.empty() || uri.path[0] != '/') {
            error = "file URI path must be absolute";
            return icResourceReadResult::Error;
        }
        std::string path;
        for (std::size_t i = 0; i < uri.path.size(); i++) {
            char c = uri.path[i];
            if (c == '%') {
                c = (char)(hex(uri.path[i + 1]) * 16 + hex(uri.path[i + 2]));
                i += 2;
                if (c == '/' || c == '\\') {
                    error = "Encoded file separators are not supported";
                    return icResourceReadResult::Error;
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
            error = "Cannot open " + uri.Document();
            return failure == ENOENT || failure == ENOTDIR ? icResourceReadResult::NotFound
                                                           : icResourceReadResult::Error;
        }
        auto length = file.tellg();
        if (length < 0 || length > 256ll * 1024 * 1024) {
            error = "Resource exceeds 256 MiB read limit";
            return icResourceReadResult::Error;
        }
        std::vector<unsigned char> next((std::size_t)length);
        file.seekg(0);
        if (length && !file.read((char *)next.data(), length)) {
            error = "Cannot read " + uri.Document();
            return icResourceReadResult::Error;
        }
        out = std::move(next);
        return icResourceReadResult::Success;
    }
};
} // namespace
std::string icResourceURI::String() const {
    return (scheme.empty() ? "" : scheme + ":") + (hasAuthority ? "//" + authority : "") + path +
           (hasQuery ? "?" + query : "") + (hasFragment ? "#" + fragment : "");
}
std::string icResourceURI::Document() const {
    auto copy = *this;
    copy.hasFragment = false;
    copy.fragment.clear();
    return copy.String();
}
bool icResourceURI::Parse(const std::string &input, icResourceURI &output, std::string &error) {
    error.clear();
    icResourceURI uri;
    std::string text = input;
    auto hash = text.find('#');
    if (hash != std::string::npos) {
        uri.hasFragment = true;
        if (!escapeNormalize(text.substr(hash + 1), uri.fragment, error))
            return false;
        text.resize(hash);
    }
    auto question = text.find('?');
    if (question != std::string::npos) {
        uri.hasQuery = true;
        if (!escapeNormalize(text.substr(question + 1), uri.query, error))
            return false;
        text.resize(question);
    }
    auto colon = text.find(':'), slash = text.find('/');
    if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
        uri.scheme = lower(text.substr(0, colon));
        if (uri.scheme.empty() || !std::isalpha((unsigned char)uri.scheme[0])) {
            error = "Invalid URI scheme";
            return false;
        }
        for (char c : uri.scheme)
            if (!std::isalnum((unsigned char)c) && c != '+' && c != '-' && c != '.') {
                error = "Invalid URI scheme";
                return false;
            }
        text.erase(0, colon + 1);
    }
    if (text.compare(0, 2, "//") == 0) {
        uri.hasAuthority = true;
        auto end = text.find('/', 2);
        std::string authority = text.substr(2, end == std::string::npos ? end : end - 2);
        if (!escapeNormalize(authority, uri.authority, error))
            return false;
        text = end == std::string::npos ? "" : text.substr(end);
    }
    if (!escapeNormalize(text, uri.path, error))
        return false;
    if (uri.hasAuthority || (!uri.path.empty() && uri.path[0] == '/'))
        uri.path = dots(uri.path);
    output = std::move(uri);
    return true;
}
bool icResourceURI::Resolve(const icResourceURI &base, const std::string &reference,
                            icResourceURI &output, std::string &error) {
    icResourceURI ref;
    if (!Parse(reference, ref, error))
        return false;
    if (!ref.scheme.empty()) {
        output = ref;
        return true;
    }
    auto result = ref;
    result.scheme = base.scheme;
    if (!ref.hasAuthority) {
        result.hasAuthority = base.hasAuthority;
        result.authority = base.authority;
        if (ref.path.empty()) {
            result.path = base.path;
            if (!ref.hasQuery) {
                result.hasQuery = base.hasQuery;
                result.query = base.query;
            }
        } else if (ref.path[0] != '/') {
            auto slash = base.path.find_last_of('/');
            result.path = dots((slash == std::string::npos ? (base.hasAuthority ? "/" : "")
                                                           : base.path.substr(0, slash + 1)) +
                               ref.path);
        }
    }
    output = std::move(result);
    return true;
}
bool icResourceURI::FromPath(const std::string &input, icResourceURI &output, std::string &error) {
    std::string path = input;
#ifdef _WIN32
    char absolute[4096];
    if (!GetFullPathNameA(path.c_str(), sizeof(absolute), absolute, nullptr)) {
        error = "Cannot resolve native path";
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
            error = "Cannot resolve current directory";
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
    return Parse("file://" + encoded, output, error);
}
icResourceResolver::icResourceResolver() { Register("file", std::make_shared<FileHandler>()); }
void icResourceResolver::Register(const std::string &scheme,
                                  std::shared_ptr<icVirtualSchemeHandler> handler) {
    handlers[lower(scheme)] = std::move(handler);
}
bool icResourceResolver::Canonical(const std::string &text, icResourceURI &uri,
                                   std::string &error) const {
    auto colon = text.find(':');
    bool native =
        colon == std::string::npos || (colon == 1 && std::isalpha((unsigned char)text[0]));
    return native ? icResourceURI::FromPath(text, uri, error)
                  : icResourceURI::Parse(text, uri, error);
}
bool icResourceResolver::Mount(const std::string &from, const std::string &to, std::string &error) {
    return MountSearch(from, {to}, error);
}
bool icResourceResolver::MountSearch(const std::string &from,
                                     const std::vector<std::string> &targets, std::string &error) {
    icResourceURI a;
    if (!icResourceURI::Parse(from, a, error))
        return false;
    auto root = [](const icResourceURI &u) {
        return !u.scheme.empty() && !u.path.empty() && u.path.back() == '/' && !u.hasQuery &&
               !u.hasFragment;
    };
    if (!root(a) || targets.empty()) {
        error = "Mounts require absolute URI directory roots and at least one target";
        return false;
    }
    std::vector<std::string> next;
    for (auto &target : targets) {
        icResourceURI b;
        if (!icResourceURI::Parse(target, b, error))
            return false;
        if (!root(b)) {
            error = "Mount targets require absolute URI directory roots without query/fragment";
            return false;
        }
        next.push_back(b.String());
    }
    mounts[a.String()] = std::move(next);
    return true;
}
bool icResourceResolver::Read(const std::string &text, std::vector<unsigned char> &bytes,
                              std::string &error) const {
    error.clear();
    icResourceURI uri;
    if (!Canonical(text, uri, error))
        return false;
    uri.hasFragment = false;
    uri.fragment.clear();
    std::set<std::string> active;
    return ReadResolved(uri, bytes, error, active, 0) == icResourceReadResult::Success;
}
icResourceReadResult icResourceResolver::ReadResolved(const icResourceURI &uri,
                                                      std::vector<unsigned char> &bytes,
                                                      std::string &error,
                                                      std::set<std::string> &active,
                                                      unsigned depth) const {
    if (depth >= 32) {
        error = "URI mount chain exceeds 32 entries";
        return icResourceReadResult::Error;
    }
    auto key = uri.Document();
    if (!active.insert(key).second) {
        error = "URI mount cycle: " + key;
        return icResourceReadResult::Error;
    }
    struct Pop {
        std::set<std::string> &set;
        std::string key;
        ~Pop() { set.erase(key); }
    } pop{active, key};
    const std::pair<const std::string, std::vector<std::string>> *match = nullptr;
    for (auto &mount : mounts)
        if (key.compare(0, mount.first.size(), mount.first) == 0 &&
            (!match || mount.first.size() > match->first.size()))
            match = &mount;
    if (match) {
        std::string searched = "Resource not found: " + key;
        for (auto &target : match->second) {
            icResourceURI candidate;
            if (!icResourceURI::Parse(target + key.substr(match->first.size()), candidate, error))
                return icResourceReadResult::Error;
            std::string reason;
            auto result = ReadResolved(candidate, bytes, reason, active, depth + 1);
            if (result == icResourceReadResult::Success) {
                error.clear();
                return result;
            }
            if (result == icResourceReadResult::Error) {
                error = reason;
                return result;
            }
            searched += "\n  " + reason;
        }
        error = searched;
        return icResourceReadResult::NotFound;
    }
    auto handler = handlers.find(uri.scheme);
    if (handler == handlers.end() || !handler->second) {
        error = "No URI scheme handler for " + uri.scheme;
        return icResourceReadResult::Error;
    }
    std::vector<unsigned char> next;
    auto result = handler->second->Read(uri, next, error);
    if (result == icResourceReadResult::Success) {
        bytes = std::move(next);
        error.clear();
    }
    return result;
}
bool icResourceResolver::LoadProject(const std::string &project, std::string &error) {
    icResourceURI base;
    if (!Canonical(project, base, error))
        return false;
    std::vector<unsigned char> bytes;
    if (!Read(base.String(), bytes, error))
        return false;
    icResourceResolver next = *this;
    std::istringstream input(std::string(bytes.begin(), bytes.end()));
    std::string line;
    unsigned number = 0;
    bool search = false;
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
            error = "Invalid project mount at line " + std::to_string(number);
            return false;
        }
        auto a = trim(line.substr(0, eq)), b = trim(line.substr(eq + 1));
        icResourceURI target;
        if (!icResourceURI::Resolve(base, b, target, error))
            return false;
        if (search)
            searches[a].push_back(target.String());
        else if (!next.Mount(a, target.String(), error))
            return false;
    }
    for (auto &entry : searches)
        if (!next.MountSearch(entry.first, entry.second, error))
            return false;
    mounts = std::move(next.mounts);
    return true;
}
