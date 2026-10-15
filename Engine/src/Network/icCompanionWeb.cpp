#include "Network/icCompanionWeb.h"
#include "Network/icCompanionLink.h"
#include "icCompanionPage.h"
#include <array>
#include <map>
#include <random>
#include <sstream>
#include <cctype>
#include <cmath>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using WebSocket=SOCKET;
static const WebSocket invalidWeb=INVALID_SOCKET;
static void closeWeb(WebSocket s){closesocket(s);}
static bool nonblocking(WebSocket s){u_long enabled=1;return ioctlsocket(s,FIONBIO,&enabled)==0;}
static bool wouldBlock(){return WSAGetLastError()==WSAEWOULDBLOCK;}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
using WebSocket=int;
static const WebSocket invalidWeb=-1;
static void closeWeb(WebSocket s){close(s);}
static bool nonblocking(WebSocket s){return fcntl(s,F_SETFL,O_NONBLOCK)==0;}
static bool wouldBlock(){return errno==EAGAIN||errno==EWOULDBLOCK;}
#endif
namespace {
std::string trim(std::string s){auto a=s.find_first_not_of(" \t");auto b=s.find_last_not_of(" \t");return a==std::string::npos?"":s.substr(a,b-a+1);}
std::string jsonText(const std::string& text){std::string result;for(unsigned char ch:text){if(ch=='"'||ch=='\\')result+='\\';if(ch>=32)result+=(char)ch;}return result;}
std::string token(){std::random_device random;std::ostringstream text;text<<std::hex;for(int i=0;i<4;++i){text.width(8);text.fill('0');text<<random();}return text.str();}
}
struct icCompanionWeb::Impl {
 struct Client {WebSocket socket=invalidWeb;std::string input,output;std::size_t sent=0;double began=0;};
 WebSocket listener=invalidWeb;std::array<Client,8> clients;
 icCompanionLink upstream;unsigned short port=0,gamePort=0;unsigned code=0,lastSequence=0,attempts=0;
 std::string session;double lastSeen=0,lastPoll=-1,attemptWindow=0;
 bool lastAccepted=false;
 void dropPeer(){upstream.Close();session.clear();lastSequence=0;lastAccepted=false;}
 void closeClient(Client& c){if(c.socket!=invalidWeb)closeWeb(c.socket);c=Client();}
 void respond(Client& c,int status,const std::string& body,const std::string& cookie="",bool html=false){
  const char* reason=status==200?"OK":status==400?"Bad Request":status==401?"Unauthorized":status==403?"Forbidden":status==404?"Not Found":status==409?"Conflict":status==413?"Content Too Large":"Too Many Requests";
  c.output="HTTP/1.1 "+std::to_string(status)+" "+reason+"\r\nContent-Type: "+(html?"text/html; charset=utf-8":"application/json")+"\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'\r\n";
  if(!cookie.empty())c.output+="Set-Cookie: ICE"+std::to_string(port)+"="+cookie+"; Path=/; HttpOnly; SameSite=Strict\r\n";
  c.output+="\r\n"+body;
 }
 bool authorized(const std::map<std::string,std::string>& h){
  if(session.empty())return false;auto found=h.find("cookie");if(found==h.end())return false;
  std::istringstream cookies(found->second);std::string entry;while(std::getline(cookies,entry,';'))if(trim(entry)=="ICE"+std::to_string(port)+"="+session)return true;return false;
 }
 void request(Client& c,double now){
  auto end=c.input.find("\r\n\r\n");if(end==std::string::npos){if(c.input.size()>4096)respond(c,413,"{}");return;}
  if(end>4096){respond(c,413,"{}");return;}
  std::istringstream head(c.input.substr(0,end));std::string first,method,path,version,extra;
  std::getline(head,first);std::istringstream line(first);line>>method>>path>>version;
  if((line>>extra)||version!="HTTP/1.1"||(method!="GET"&&method!="POST")){respond(c,400,"{}");return;}
  std::map<std::string,std::string> headers;std::string header;
  while(std::getline(head,header)){
   if(!header.empty()&&header.back()=='\r')header.pop_back();auto colon=header.find(':');
   if(colon==std::string::npos||colon==0||header[0]==' '||header[0]=='\t'){respond(c,400,"{}");return;}
   std::string key=header.substr(0,colon);for(char& ch:key){if(!std::isalnum((unsigned char)ch)&&ch!='-'){respond(c,400,"{}");return;}ch=(char)std::tolower((unsigned char)ch);}
   if(headers.count(key)){respond(c,400,"{}");return;}headers[key]=trim(header.substr(colon+1));
  }
  if(!headers.count("host")||headers.count("transfer-encoding")){respond(c,400,"{}");return;}
  // Numeric Host only, with this listener's port: blocks DNS-rebinding origins.
  const std::string host=headers["host"];auto colon=host.find(':');in_addr ip{};
  if(colon==std::string::npos||host.substr(colon+1)!=std::to_string(port)||inet_pton(AF_INET,host.substr(0,colon).c_str(),&ip)!=1){respond(c,403,"{}");return;}
  unsigned length=0;
  if(headers.count("content-length")){const auto& text=headers["content-length"];if(text.empty()||text.size()>3){respond(c,413,"{}");return;}for(char ch:text){if(ch<'0'||ch>'9'){respond(c,400,"{}");return;}length=length*10+ch-'0';}}
  if(length>128){respond(c,413,"{}");return;}
  if(method=="POST"&&(!headers.count("content-length")||headers["origin"]!="http://"+host)){respond(c,403,"{}");return;}
  if(c.input.size()<end+4+length)return;
  if(c.input.size()!=end+4+length){respond(c,400,"{}");return;}
  std::string body=c.input.substr(end+4,length);
  if(method=="GET"&&path=="/"){respond(c,200,kCompanionPage,"",true);return;}
  bool auth=authorized(headers);
  if(method=="POST"&&path=="/join"){
   if(now-attemptWindow>=1){attemptWindow=now;attempts=0;}if(++attempts>4){respond(c,429,"{}");return;}
   if(body!=std::to_string(code)){respond(c,401,"{}");return;}
   if(!session.empty()&&!auth){respond(c,409,"{}");return;}
   dropPeer();session=token();lastSeen=now;
   if(!upstream.Connect("127.0.0.1",gamePort,code,now)){dropPeer();respond(c,409,"{}");return;}
   respond(c,200,"{}",session);return;
  }
  if(!auth){respond(c,401,"{}");return;}
  lastSeen=now;
  if(method=="GET"&&path=="/status"){
   const char* state=upstream.Status()==icCompanionLink::State::Connected?"connected":upstream.Status()==icCompanionLink::State::Pairing?"pairing":"disconnected";
   respond(c,200,std::string("{\"state\":\"")+state+"\",\"delivered\":"+std::to_string(upstream.Delivered())+",\"message\":\""+jsonText(upstream.Message())+"\"}");return;
  }
  if(method=="POST"&&path=="/leave"){dropPeer();respond(c,200,"{}");return;}
  if(method=="POST"&&path=="/shot"){
   unsigned sequence=0;icShotIntent shot;std::istringstream payload(body);payload>>sequence>>shot.aim>>shot.power;
   if(!payload||payload>>extra||!sequence||!std::isfinite(shot.aim)||!std::isfinite(shot.power)||shot.aim < -1||shot.aim>1||shot.power<0||shot.power>1){respond(c,400,"{}");return;}
   if(sequence<=lastSequence){respond(c,sequence==lastSequence&&lastAccepted?200:409,"{}");return;}
   lastSequence=sequence;shot.seconds=now;lastAccepted=upstream.SendShot(shot,now);respond(c,lastAccepted?200:409,"{}");return;
  }
  respond(c,404,"{}");
 }
};
icCompanionWeb::icCompanionWeb():m(new Impl){}
icCompanionWeb::~icCompanionWeb(){for(auto& c:m->clients)m->closeClient(c);if(m->listener!=invalidWeb)closeWeb(m->listener);}
void icCompanionWeb::Close(){for(auto& c:m->clients)m->closeClient(c);if(m->listener!=invalidWeb)closeWeb(m->listener);m.reset(new Impl);}
bool icCompanionWeb::Start(unsigned short companionPort,unsigned code){
 Close();if(!companionPort||code<100000||code>999999)return false;
#ifdef _WIN32
 static const bool winsock=[](){WSADATA data;return WSAStartup(MAKEWORD(2,2),&data)==0;}();if(!winsock)return false;
#endif
 m->listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(m->listener==invalidWeb)return false;
 if(!nonblocking(m->listener)){Close();return false;}
#ifdef __APPLE__
 int yes=1;setsockopt(m->listener,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
 sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_ANY);
 if(bind(m->listener,(sockaddr*)&local,sizeof(local))<0||listen(m->listener,8)<0){Close();return false;}
#ifdef _WIN32
 int length=sizeof(local);
#else
 socklen_t length=sizeof(local);
#endif
 if(getsockname(m->listener,(sockaddr*)&local,&length)<0){Close();return false;}
 m->port=ntohs(local.sin_port);m->gamePort=companionPort;m->code=code;return true;
}
void icCompanionWeb::Poll(double now){
 if(m->listener==invalidWeb||!std::isfinite(now)||now<0)return;
 if(m->lastPoll>=0&&(now<m->lastPoll||now-m->lastPoll>1)){m->dropPeer();for(auto& c:m->clients)m->closeClient(c);}
 m->lastPoll=now;if(!m->session.empty()&&now-m->lastSeen>2)m->dropPeer();m->upstream.Poll(now);
 for(int i=0;i<8;++i){auto socket=accept(m->listener,nullptr,nullptr);if(socket==invalidWeb)break;
  auto found=m->clients.end();for(auto c=m->clients.begin();c!=m->clients.end();++c)if(c->socket==invalidWeb){found=c;break;}
  if(found==m->clients.end()||!nonblocking(socket)){closeWeb(socket);continue;}
#ifdef __APPLE__
  int yes=1;setsockopt(socket,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
  found->socket=socket;found->began=now;
 }
 for(auto& c:m->clients){if(c.socket==invalidWeb)continue;if(now-c.began>1){m->closeClient(c);continue;}
  if(c.output.empty()){char buffer[2048];int n=(int)recv(c.socket,buffer,sizeof(buffer),0);
   if(n>0){c.input.append(buffer,n);if(c.input.size()>4230)m->respond(c,413,"{}");else m->request(c,now);}
   else if(n==0||!wouldBlock()){m->closeClient(c);continue;}
  }
  if(!c.output.empty()){
#ifdef MSG_NOSIGNAL
   const int flags=MSG_NOSIGNAL;
#else
   const int flags=0;
#endif
   int n=(int)send(c.socket,c.output.data()+c.sent,(int)(c.output.size()-c.sent),flags);
   if(n>0){c.sent+=n;if(c.sent==c.output.size())m->closeClient(c);}else if(n==0||!wouldBlock())m->closeClient(c);
  }
 }
}
unsigned short icCompanionWeb::Port()const{return m->port;}
std::string icCompanionWeb::JoinURL(const std::string& address)const{return "http://"+address+":"+std::to_string(m->port)+"/#"+std::to_string(m->code);}
