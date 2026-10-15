#include "Network/icCompanionLink.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
using Socket=SOCKET;
static const Socket bad=INVALID_SOCKET;
static void closeSocket(Socket s){closesocket(s);}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <unistd.h>
using Socket=int;
static const Socket bad=-1;
static void closeSocket(Socket s){close(s);}
#endif
namespace {
enum Kind { Hello=1, Welcome, Ready, Paired, Shot, Ack, Ping, Pong, Rejected };
struct Packet { unsigned kind=0; std::uint64_t session=0,nonce=0; unsigned seq=0; int aim=0; unsigned power=0,source=0; };
using Wire=std::array<unsigned char,40>;
void put(unsigned char* p,std::uint64_t v,int n){for(int i=n-1;i>=0;--i){p[i]=v&255;v>>=8;}}
std::uint64_t get(const unsigned char* p,int n){std::uint64_t v=0;while(n--)v=(v<<8)|*p++;return v;}
Wire encode(const Packet& p){Wire w{};std::memcpy(w.data(),"ICEC",4);w[4]=1;w[5]=p.kind;w[7]=1; // shot-intent capability
 put(w.data()+8,p.session,8);put(w.data()+16,p.nonce,8);put(w.data()+24,p.seq,4);put(w.data()+28,p.aim+10000,2);put(w.data()+30,p.power,2);w[32]=p.source;return w;}
bool decode(const unsigned char* w,int n,Packet& p){
 if(n!=40||std::memcmp(w,"ICEC",4)||w[4]!=1||w[5]<Hello||w[5]>Rejected||w[6]||w[7]!=1)return false;
 for(int i=33;i<40;++i)if(w[i])return false;
 p.kind=w[5];p.session=get(w+8,8);p.nonce=get(w+16,8);p.seq=(unsigned)get(w+24,4);p.aim=(int)get(w+28,2)-10000;p.power=(unsigned)get(w+30,2);p.source=w[32];
 return p.nonce && p.aim>=-10000 && p.aim<=10000 && p.power<=10000 && p.source<=1;
}
bool same(const sockaddr_in& a,const sockaddr_in& b){return a.sin_port==b.sin_port&&a.sin_addr.s_addr==b.sin_addr.s_addr;}
std::uint64_t randomID(){std::random_device r;std::uint64_t v=(std::uint64_t(r())<<32)^r();return v?v:1;}
}
struct icCompanionLink::Impl {
 Socket socket=bad;State state=State::Closed;std::string message="Local test / disconnected";
 sockaddr_in peer{};bool server=false,accept=false,pending=false,queued=false,ready=false,lastRejected=false;
 unsigned code=0,seq=0,lastSeq=0,delivered=0,attempts=0;unsigned short port=0;
 std::uint64_t session=0,nonce=0;double heard=0,sent=-100,start=0,pendingAt=0,queuedAt=0,attemptWindow=0,lastPoll=-1;
 Packet retry;icShotIntent queuedShot;
 bool open(const char* address,unsigned short requested){
#ifdef _WIN32
  static const bool winsock=[](){WSADATA data;return WSAStartup(MAKEWORD(2,2),&data)==0;}();if(!winsock)return false;
#endif
  sockaddr_in local{};local.sin_family=AF_INET;local.sin_port=htons(requested);
  if(inet_pton(AF_INET,address,&local.sin_addr)!=1)return false;
  socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socket==bad)return false;
#ifdef _WIN32
  u_long nonblocking=1;if(ioctlsocket(socket,FIONBIO,&nonblocking)){closeSocket(socket);socket=bad;return false;}
#else
  if(fcntl(socket,F_SETFL,O_NONBLOCK)<0){closeSocket(socket);socket=bad;return false;}
#endif
  if(bind(socket,(sockaddr*)&local,sizeof(local))<0){closeSocket(socket);socket=bad;return false;}
#ifdef _WIN32
  int length=sizeof(local);
#else
  socklen_t length=sizeof(local);
#endif
  if(getsockname(socket,(sockaddr*)&local,&length)<0){closeSocket(socket);socket=bad;return false;}port=ntohs(local.sin_port);return true;
 }
 void send(Packet p,const sockaddr_in& to){auto w=encode(p);sendto(socket,(const char*)w.data(),(int)w.size(),0,(const sockaddr*)&to,sizeof(to));}
 void send(unsigned kind,unsigned number=0){Packet p;p.kind=kind;p.session=session;p.nonce=nonce;p.seq=number;send(p,peer);}
 void resetPeer(){session=nonce=0;seq=lastSeq=0;pending=queued=ready=false;heard=0;sent=-100;}
};
icCompanionLink::icCompanionLink():m(new Impl){}
icCompanionLink::~icCompanionLink(){if(m->socket!=bad)closeSocket(m->socket);}
void icCompanionLink::Close(){if(m->socket!=bad)closeSocket(m->socket);m.reset(new Impl);}
bool icCompanionLink::Listen(unsigned short port,unsigned code,const char* address){Close();if(code<100000||code>999999||!m->open(address,port)){m->state=State::Failed;m->message="Could not open companion port";return false;}m->server=true;m->code=code;m->state=State::Listening;m->message="Waiting for companion";return true;}
bool icCompanionLink::Connect(const char* ip,unsigned short port,unsigned code,double now){
 Close();m->peer.sin_family=AF_INET;m->peer.sin_port=htons(port);
 if(!std::isfinite(now)||now<0||!port||code<100000||code>999999||inet_pton(AF_INET,ip,&m->peer.sin_addr)!=1||!m->open("0.0.0.0",0)){
  m->state=State::Failed;m->message="Enter IPv4 address, port and six-digit code";return false;}
 m->nonce=randomID();m->code=code;m->start=m->heard=now;m->state=State::Pairing;m->message="Pairing with desktop";Poll(now);return true;
}
void icCompanionLink::AcceptShots(bool accept){m->accept=accept;if(!accept)m->queued=false;}
void icCompanionLink::Poll(double now){
 if(m->socket==bad||!std::isfinite(now)||now<0)return;
 // A paused loop must not consume buffered actions from before suspension.
 if(m->lastPoll>=0 && (now<m->lastPoll||now-m->lastPoll>1.0)){
  if(m->server){m->resetPeer();m->state=State::Listening;m->message="Companion disconnected";}
  else{Close();m->message="Connection paused; reconnect";return;}
 }
 m->lastPoll=now;
 if(m->queued&&now-m->queuedAt>.35)m->queued=false;
 if(m->session&&now-m->heard>3){m->resetPeer();m->state=m->server?State::Listening:State::Failed;m->message="Companion timed out; reconnect";}
 // Bound packet processing per frame, including malformed datagrams.
 for(int count=0;count<64;++count){unsigned char bytes[512];sockaddr_in from{};
#ifdef _WIN32
  int length=sizeof(from);
#else
  socklen_t length=sizeof(from);
#endif
  int n=(int)recvfrom(m->socket,(char*)bytes,sizeof(bytes),0,(sockaddr*)&from,&length);if(n<0)break;
  Packet p;if(!decode(bytes,n,p))continue;
  if(m->server&&p.kind==Hello){
   if(now-m->attemptWindow>=1){m->attemptWindow=now;m->attempts=0;}if(++m->attempts>8)continue;
   if(p.seq!=m->code||p.session)continue;
   if(m->session){if(!same(from,m->peer)||p.nonce!=m->nonce)continue;}
   else{m->peer=from;m->nonce=p.nonce;m->session=randomID();m->heard=now;m->state=State::Pairing;}
   m->send(Welcome);continue;
  }
  if(!same(from,m->peer)||p.nonce!=m->nonce)continue;
  if(!m->server&&p.kind==Welcome&&m->state==State::Pairing&&p.session){
   if(m->session&&m->session!=p.session)continue;m->session=p.session;m->heard=now;m->ready=true;m->send(Ready);continue;
  }
  if(!p.session||p.session!=m->session)continue;
  if(m->server&&p.kind==Ready){m->state=State::Connected;m->message="Companion connected";m->heard=now;m->send(Paired);continue;}
  if(!m->server&&p.kind==Paired&&m->state==State::Pairing){m->state=State::Connected;m->message="Connected to desktop";m->heard=now;continue;}
  if(m->state!=State::Connected)continue;
  if(m->server&&p.kind==Shot&&p.seq){
   m->heard=now;
   if(p.seq<=m->lastSeq){m->send(m->lastRejected?Rejected:Ack,p.seq);continue;}
   m->lastSeq=p.seq;m->lastRejected=!m->accept||m->queued;
   if(!m->accept||m->queued){m->send(Rejected,p.seq);continue;}
   m->queuedShot.aim=p.aim/10000.f;m->queuedShot.power=p.power/10000.f;m->queuedShot.source=(icShotMode)p.source;m->queuedShot.seconds=now;m->queued=true;m->queuedAt=now;m->send(Ack,p.seq);continue;
  }
  if(!m->server&&(p.kind==Ack||p.kind==Rejected)&&m->pending&&p.seq==m->retry.seq){m->heard=now;m->pending=false;if(p.kind==Ack){++m->delivered;m->message="Shot received by desktop";}else m->message="Desktop not ready for a shot";continue;}
  if(m->server&&p.kind==Ping){m->heard=now;m->send(Pong);}
  if(!m->server&&p.kind==Pong)m->heard=now;
 }
 if(!m->server&&m->state==State::Pairing){
  if(now-m->start>5){m->state=State::Failed;m->message="Pairing timed out; check address and code";return;}
  if(now-m->sent>=.2){m->send(m->ready?Ready:Hello,m->ready?0:m->code);m->sent=now;}
 }
 if(!m->server&&m->state==State::Connected){
  if(m->pending&&now-m->pendingAt>.35){m->pending=false;m->message="No confirmation; shot may have arrived";}
  if(m->pending&&now-m->sent>=.06){m->send(m->retry,m->peer);m->sent=now;}
  else if(!m->pending&&now-m->sent>=.5){m->send(Ping);m->sent=now;}
 }
}
bool icCompanionLink::SendShot(const icShotIntent& shot,double now){
 if(m->server||m->state!=State::Connected||m->pending||!std::isfinite(now)||now<0||!std::isfinite(shot.aim)||!std::isfinite(shot.power)||shot.aim < -1||shot.aim>1||shot.power<0||shot.power>1||(shot.source!=icShotMode::Touch&&shot.source!=icShotMode::Motion))return false;
 if(m->seq==std::numeric_limits<unsigned>::max()){Close();return false;}
 Packet p;p.kind=Shot;p.session=m->session;p.nonce=m->nonce;p.seq=++m->seq;p.aim=(int)std::lround(shot.aim*10000);p.power=(unsigned)std::lround(shot.power*10000);p.source=(unsigned)shot.source;
 m->retry=p;m->pending=true;m->pendingAt=m->sent=now;m->send(p,m->peer);return true;
}
bool icCompanionLink::TakeShot(icShotIntent& shot,double now){if(!m->server||!m->queued||!std::isfinite(now)||now<m->queuedAt)return false;if(now-m->queuedAt>.35){m->queued=false;return false;}shot=m->queuedShot;m->queued=false;return true;}
icCompanionLink::State icCompanionLink::Status()const{return m->state;}
const std::string& icCompanionLink::Message()const{return m->message;}
unsigned short icCompanionLink::Port()const{return m->port;}
std::uint64_t icCompanionLink::PeerDevice()const{return m->state==State::Connected?m->session:0;}
unsigned icCompanionLink::Delivered()const{return m->delivered;}

std::string icCompanionLink::LocalAddresses(){
 std::string result;
 auto add=[&](const sockaddr* address){if(!address||address->sa_family!=AF_INET)return;auto* ip=(const sockaddr_in*)address;unsigned value=ntohl(ip->sin_addr.s_addr);if(!value||(value>>24)==127||(value>>16)==0xa9fe)return;char text[INET_ADDRSTRLEN];if(inet_ntop(AF_INET,&ip->sin_addr,text,sizeof(text))){if(!result.empty())result+=" / ";result+=text;}};
#ifdef _WIN32
 ULONG size=16384;std::vector<unsigned char> buffer(size);auto* adapters=(IP_ADAPTER_ADDRESSES*)buffer.data();
 if(GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,nullptr,adapters,&size)==ERROR_BUFFER_OVERFLOW){buffer.resize(size);adapters=(IP_ADAPTER_ADDRESSES*)buffer.data();}
 if(GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,nullptr,adapters,&size)==NO_ERROR)for(auto* a=adapters;a;a=a->Next)if(a->OperStatus==IfOperStatusUp)for(auto* u=a->FirstUnicastAddress;u;u=u->Next)add(u->Address.lpSockaddr);
#else
 ifaddrs* addresses=nullptr;if(getifaddrs(&addresses)==0){for(auto* a=addresses;a;a=a->ifa_next)add(a->ifa_addr);freeifaddrs(addresses);}
#endif
 return result.empty()?"127.0.0.1":result;
}
