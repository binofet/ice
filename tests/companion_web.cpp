#include "Network/icCompanionWeb.h"
#include "Network/icCompanionLink.h"
#include <chrono>
#include <thread>
#include <future>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using TestWebSocket=SOCKET;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
using TestWebSocket=int;
#endif
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static std::string exchange(unsigned short port,const std::string& request){
 auto s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
#ifdef _WIN32
 DWORD timeout=3000;
#else
 timeval timeout{3,0};
#endif
 setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
 if(connect(s,(sockaddr*)&a,sizeof(a))<0)return "";
 // Split requests exercise incremental parsing instead of assuming one recv.
 auto half=request.size()/2;send(s,request.data(),(int)half,0);std::this_thread::sleep_for(std::chrono::milliseconds(3));send(s,request.data()+half,(int)(request.size()-half),0);
 std::string response;char buffer[2048];for(;;){int n=recv(s,buffer,sizeof(buffer),0);if(n<=0)break;response.append(buffer,n);}
#ifdef _WIN32
 closesocket(s);
#else
 close(s);
#endif
 return response;
}
int main(){
 icCompanionLink host;check(host.Listen(0,123456,"127.0.0.1"),"native host");host.AcceptShots(true);icCompanionWeb web;check(web.Start(host.Port(),123456),"web bridge");
 std::string authority="127.0.0.1:"+std::to_string(web.Port());
 auto pump=[&](int count){for(int i=0;i<count;++i){double t=now();web.Poll(t);host.Poll(t);std::this_thread::sleep_for(std::chrono::milliseconds(2));}};
 auto raw=[&](const std::string& request){auto result=std::async(std::launch::async,[&](){return exchange(web.Port(),request);});double start=now();while(result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready&&now()-start<4)pump(1);check(result.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,"bounded HTTP response");return result.get();};
 auto request=[&](const char* method,const char* path,const std::string& body,const std::string& cookie){return raw(std::string(method)+" "+path+" HTTP/1.1\r\nHost: "+authority+"\r\nOrigin: http://"+authority+"\r\nContent-Length: "+std::to_string(body.size())+"\r\n"+(cookie.empty()?"":"Cookie: "+cookie+"\r\n")+"\r\n"+body);};
 auto page=request("GET","/","","");check(page.find("HTTP/1.1 200")==0&&page.find("SkeeBall Controller")!=std::string::npos,"embedded controller served");
 check(request("POST","/join","654321","").find("HTTP/1.1 401")==0,"wrong code denied");
 auto join=request("POST","/join","123456","");check(join.find("HTTP/1.1 200")==0,"browser joins");auto c=join.find("Set-Cookie: ");check(c!=std::string::npos,"session cookie");c+=12;std::string cookie=join.substr(c,join.find(';',c)-c);
 pump(20);check(request("GET","/status","",cookie).find("\"connected\"")!=std::string::npos,"browser-to-native handshake");
 check(request("POST","/join","123456","").find("HTTP/1.1 409")==0,"another browser cannot replace active controller");
 check(request("POST","/shot","1 0.4 0.6",cookie).find("HTTP/1.1 200")==0,"touch intent queued");pump(10);icShotIntent shot;check(host.TakeShot(shot,now())&&shot.aim>.399&&shot.power>.599,"browser intent reaches shared native host");
 check(request("POST","/shot","1 0.4 0.6",cookie).find("HTTP/1.1 200")==0,"duplicate HTTP command retains result");pump(10);check(!host.TakeShot(shot,now()),"duplicate HTTP command cannot launch twice");
 check(request("POST","/shot","2 8 0.6",cookie).find("HTTP/1.1 400")==0,"invalid aim rejected");
 check(request("POST","/shot","2 0.4 0.6","ICE=bad").find("HTTP/1.1 401")==0,"session required");
 check(raw("POST /join HTTP/1.1\r\nHost: "+authority+"\r\nOrigin: http://unrelated.invalid\r\nContent-Length: 6\r\n\r\n123456").find("HTTP/1.1 403")==0,"cross-origin commands rejected");
 check(raw("POST /join HTTP/1.1\r\nHost: "+authority+"\r\nContent-Length: 6\r\nContent-Length: 6\r\n\r\n123456").find("HTTP/1.1 400")==0,"ambiguous request framing rejected");
 check(request("POST","/leave","",cookie).find("HTTP/1.1 200")==0,"browser leaves");check(request("POST","/shot","2 0.4 0.6",cookie).find("HTTP/1.1 401")==0,"old tab cannot send after leaving");
 auto rejoin=request("POST","/join","123456","");check(rejoin.find("HTTP/1.1 200")==0,"rejoin with a fresh browser session");auto next=rejoin.find("Set-Cookie: ")+12;std::string fresh=rejoin.substr(next,rejoin.find(';',next)-next);check(fresh!=cookie,"browser session token rotates");
 web.Poll(now()+2);check(request("POST","/shot","1 0.4 0.6",fresh).find("HTTP/1.1 401")==0,"paused host invalidates buffered browser authority");
 std::puts("Browser controller HTTP/session/UDP tests passed");
}
