#ifndef IC_COMPANION_WEB_H
#define IC_COMPANION_WEB_H
#include <memory>
#include <string>
// Optional local HTTP touch controller. Owns a native client connection to the
// existing companion UDP host; gameplay does not depend on a browser transport.
class icCompanionWeb {
public:
    icCompanionWeb();
    ~icCompanionWeb();
    icCompanionWeb(const icCompanionWeb&)=delete;
    icCompanionWeb& operator=(const icCompanionWeb&)=delete;
    bool Start(unsigned short companionPort,unsigned code);
    void Poll(double now);
    void Close();
    unsigned short Port() const;
    std::string JoinURL(const std::string& address) const;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
#endif
