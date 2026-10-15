#pragma once
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// Hierarchical resource URI. Parsing normalizes scheme/dot segments and preserves
// escaped reserved characters. Native paths enter only through FromPath().
struct icResourceURI {
    std::string scheme, authority, path, query, fragment;
    bool hasAuthority = false, hasQuery = false, hasFragment = false;
    std::string String() const;
    std::string Document() const;
    static bool Parse(const std::string &, icResourceURI &, std::string &diagnostics);
    static bool Resolve(const icResourceURI &base, const std::string &reference, icResourceURI &,
                        std::string &diagnostics);
    static bool FromPath(const std::string &, icResourceURI &, std::string &diagnostics);
};
enum class icResourceReadResult { Success, NotFound, Error };
class icVirtualSchemeHandler {
  public:
    virtual ~icVirtualSchemeHandler() {}
    virtual icResourceReadResult Read(const icResourceURI &, std::vector<unsigned char> &bytes,
                                      std::string &diagnostics) = 0;
};
class icResourceResolver {
  public:
    icResourceResolver(); // registers file://; no network access or implicit HTTP requests
    void Register(const std::string &scheme, std::shared_ptr<icVirtualSchemeHandler>);
    bool Mount(const std::string &virtualRoot, const std::string &targetRoot,
               std::string &diagnostics);
    // First existing resource wins. Only NotFound permits trying the next root.
    bool MountSearch(const std::string &virtualRoot, const std::vector<std::string> &targetRoots,
                     std::string &diagnostics);
    bool LoadProject(const std::string &projectURIOrPath, std::string &diagnostics);
    bool Read(const std::string &uri, std::vector<unsigned char> &bytes,
              std::string &diagnostics) const;
    bool Canonical(const std::string &uriOrPath, icResourceURI &, std::string &diagnostics) const;

  private:
    std::map<std::string, std::shared_ptr<icVirtualSchemeHandler>> handlers;
    std::map<std::string, std::vector<std::string>> mounts;
    icResourceReadResult ReadResolved(const icResourceURI &, std::vector<unsigned char> &,
                                      std::string &, std::set<std::string> &, unsigned) const;
};
