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
 web.SetGameReady(true);
 auto cookieOf=[](const std::string& r){auto c=r.find("Set-Cookie: ");check(c!=std::string::npos,"session cookie");c+=12;return r.substr(c,r.find(';',c)-c);};
 auto page=request("GET","/","","");check(page.find("HTTP/1.1 200")==0,"embedded controller served");
 check(request("POST","/join","654321","").find("HTTP/1.1 401")==0,"wrong code denied");
 auto alice=cookieOf(request("POST","/join","123456\nAlice",""));
 auto bob=cookieOf(request("POST","/join","123456\nBob",""));
 pump(20);check(web.Room().Players().size()==2,"independent browser seats");
 check(request("POST","/start","",bob).find("HTTP/1.1 409")==0,"only leader starts");
 check(request("POST","/start","",alice).find("HTTP/1.1 200")==0,"leader starts round");
 auto turn=web.Room().Turn();
 auto shotBody=[&](unsigned seq,unsigned generation){return std::to_string(seq)+" "+std::to_string(generation)+" 0.4 0.6";};
 check(request("POST","/shot",shotBody(1,turn),bob).find("HTTP/1.1 409")==0,"out of turn denied");
 check(request("POST","/shot",shotBody(1,turn),alice).find("HTTP/1.1 200")==0,"current player queues shot");pump(10);
 icShotIntent shot;check(host.TakeShot(shot,now())&&shot.aim>.399&&shot.power>.599,"intent reaches native host");
 auto owner=web.ClaimShotOwner(now());check(owner==web.Room().Players()[0].id,"shot ownership retained");
 check(request("POST","/shot",shotBody(1,turn),alice).find("HTTP/1.1 200")==0,"duplicate retains result");pump(10);check(!host.TakeShot(shot,now()),"duplicate cannot launch twice");
 check(web.FinishShot(owner,50),"settle owned shot");check(web.Room().Players()[0].score==50,"score belongs to shooter");
 check(request("POST","/shot",shotBody(2,turn),bob).find("HTTP/1.1 409")==0,"stale turn denied");
 check(request("POST","/shot",shotBody(3,web.Room().Turn()),bob).find("HTTP/1.1 200")==0,"next player shoots");pump(10);check(host.TakeShot(shot,now()),"second intent reaches host");owner=web.ClaimShotOwner(now());
 check(request("POST","/leave","",bob).find("HTTP/1.1 200")==0,"player leaves during flight");check(web.FinishShot(owner,20),"disconnected shooter still scores");check(web.Room().Players()[1].score==20,"disconnected ownership preserved");
 check(request("POST","/shot",shotBody(4,web.Room().Turn()),bob).find("HTTP/1.1 401")==0,"left session cannot shoot");
 check(raw("POST /join HTTP/1.1\r\nHost: "+authority+"\r\nOrigin: http://unrelated.invalid\r\nContent-Length: 6\r\n\r\n123456").find("HTTP/1.1 403")==0,"cross-origin denied");
 check(raw("POST /join HTTP/1.1\r\nHost: "+authority+"\r\nContent-Length: 6\r\nContent-Length: 6\r\n\r\n123456").find("HTTP/1.1 400")==0,"ambiguous framing denied");
 web.Poll(now()+2);check(request("GET","/status","",alice).find("HTTP/1.1 401")==0,"host suspension invalidates authority");
 std::puts("Party HTTP/session/UDP/ownership tests passed");
}
