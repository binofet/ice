#include "Network/icCompanionLink.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <thread>
#include <chrono>
static void check(bool ok,const char* reason){if(!ok){std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1);}}
static void pump(icCompanionLink& host,icCompanionLink& client,double& now,int count=8){for(int i=0;i<count;++i){now+=.01;client.Poll(now);std::this_thread::sleep_for(std::chrono::milliseconds(2));host.Poll(now);}}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using TestSocket=SOCKET;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
using TestSocket=int;
#endif
// A real UDP fault proxy: drops replies and duplicates shot datagrams without
// reaching inside the transport's state or reducer implementation.
struct FaultProxy {
 TestSocket socket; sockaddr_in host{},client{};unsigned short port;bool dropReplies=false,duplicateShots=false;
 explicit FaultProxy(unsigned short hostPort){
  socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  check(bind(socket,(sockaddr*)&local,sizeof(local))==0,"proxy bind");
#ifdef _WIN32
  u_long on=1;ioctlsocket(socket,FIONBIO,&on);int size=sizeof(local);
#else
  fcntl(socket,F_SETFL,O_NONBLOCK);socklen_t size=sizeof(local);
#endif
  getsockname(socket,(sockaddr*)&local,&size);port=ntohs(local.sin_port);host=local;host.sin_port=htons(hostPort);
 }
 ~FaultProxy(){
#ifdef _WIN32
 closesocket(socket);
#else
 close(socket);
#endif
 }
 void Poll(){for(int i=0;i<64;++i){unsigned char bytes[512];sockaddr_in from{};
#ifdef _WIN32
 int size=sizeof(from);
#else
 socklen_t size=sizeof(from);
#endif
 int n=recvfrom(socket,(char*)bytes,sizeof(bytes),0,(sockaddr*)&from,&size);if(n<0)return;
 bool reply=from.sin_port==host.sin_port;bool shot=n==40&&bytes[5]==5;
 if(reply&&dropReplies&&n==40&&(bytes[5]==6||bytes[5]==9))continue;
 if(!reply)client=from;const sockaddr_in& to=reply?client:host;
 sendto(socket,(const char*)bytes,n,0,(const sockaddr*)&to,sizeof(to));
 if(shot&&duplicateShots)sendto(socket,(const char*)bytes,n,0,(const sockaddr*)&to,sizeof(to));
 }}
};
static void pumpFaults(icCompanionLink& host,icCompanionLink& client,FaultProxy& proxy,double& now,int count=12){
 for(int i=0;i<count;++i){now+=.01;client.Poll(now);std::this_thread::sleep_for(std::chrono::milliseconds(2));proxy.Poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));host.Poll(now);std::this_thread::sleep_for(std::chrono::milliseconds(2));proxy.Poll();}
}
int main(){
 using State=icCompanionLink::State;icCompanionLink host,client;double now=10;
 check(host.Listen(0,123456,"127.0.0.1"),"listen ephemeral");host.AcceptShots(true);
 check(client.Connect("127.0.0.1",host.Port(),123456,now),"connect");pump(host,client,now);
  check(host.Status()==State::Connected&&client.Status()==State::Connected,"version/capability handshake");
 auto firstDevice=host.PeerDevice();check(firstDevice!=0,"remote device identity");
 icShotIntent shot;shot.aim=-.3518f;shot.power=.7234f;shot.source=icShotMode::Motion;
 check(client.SendShot(shot,now),"send shot");check(!client.SendShot(shot,now),"bounded pending shots");pump(host,client,now);
 icShotIntent received;check(host.TakeShot(received,now),"receive shot");check(!host.TakeShot(received,now),"consume once");
 check(std::fabs(received.aim-shot.aim)<.00011&&std::fabs(received.power-shot.power)<.00011&&received.source==shot.source,"portable quantized payload");check(client.Delivered()==1,"shot acknowledgement");
 host.AcceptShots(false);check(client.SendShot(shot,now),"send while game unavailable");pump(host,client,now);check(!host.TakeShot(received,now)&&client.Delivered()==1,"game rejects rather than queues a future shot");
 host.AcceptShots(true);check(client.SendShot(shot,now),"queue shot");pump(host,client,now);now+=.4;host.Poll(now);check(!host.TakeShot(received,now),"queued game action expires");
 check(client.SendShot(shot,now),"send before suspension");now+=2;host.Poll(now);check(!host.TakeShot(received,now)&&host.PeerDevice()==0,"paused host rejects buffered old session shots");client.Close();pump(host,client,now);
 check(client.Connect("127.0.0.1",host.Port(),123456,now),"reconnect");pump(host,client,now);check(host.PeerDevice()&&host.PeerDevice()!=firstDevice,"new session has new device identity");check(!host.TakeShot(received,now),"no replay on reconnect");
 client.Close();host.Close();check(host.Listen(0,123456,"127.0.0.1"),"listen again");check(client.Connect("127.0.0.1",host.Port(),654321,now),"wrong-code handshake attempt");
 for(int i=0;i<60;++i){now+=.1;client.Poll(now);std::this_thread::sleep_for(std::chrono::milliseconds(2));host.Poll(now);}check(client.Status()==State::Failed&&host.PeerDevice()==0,"wrong code cannot connect");
 check(!client.Connect("host.invalid",host.Port(),123456,now),"numeric address avoids blocking DNS");
 host.Close();client.Close();check(host.Listen(0,123456,"127.0.0.1"),"fault-test host");host.AcceptShots(true);
 FaultProxy proxy(host.Port());check(client.Connect("127.0.0.1",proxy.port,123456,now),"proxy connect");pumpFaults(host,client,proxy,now);
 check(client.Status()==State::Connected,"handshake through proxy");proxy.dropReplies=true;proxy.duplicateShots=true;
 check(client.SendShot(shot,now),"send across dropped replies");pumpFaults(host,client,proxy,now);
 check(host.TakeShot(received,now)&&!host.TakeShot(received,now),"duplicate datagrams produce one action");check(client.Delivered()==0,"lost acknowledgement remains unconfirmed");
 proxy.dropReplies=false;pumpFaults(host,client,proxy,now);check(client.Delivered()==1&&!host.TakeShot(received,now),"retransmission acknowledges without launching again");
 host.AcceptShots(false);proxy.dropReplies=true;check(client.SendShot(shot,now),"busy-game dropped rejection");pumpFaults(host,client,proxy,now);proxy.dropReplies=false;pumpFaults(host,client,proxy,now);
 check(client.Delivered()==1&&!host.TakeShot(received,now),"duplicate rejected shot stays rejected");
 std::puts("Companion link loopback and packet-loss tests passed");
}
