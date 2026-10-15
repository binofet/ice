#include "Util/icJoinQR.h"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(int argc,char** argv){icJoinQR qr;const std::string text="http://192.168.1.170:65115/#157804";
 check(qr.Encode(text),"LAN URL fits bounded QR");check(qr.dark[3][3]&&qr.dark[0][0]&&!qr.dark[1][1]&&qr.dark[6][6],"finder geometry");check(qr.dark[22][22]&&!qr.dark[21][21]&&qr.dark[20][20],"alignment geometry");check(qr.dark[21][8],"fixed dark module");
 auto first=qr.dark;check(qr.Encode(text)&&qr.dark==first,"deterministic encoding");check(qr.Encode(std::string(53,'a')),"maximum byte capacity");check(!qr.Encode(std::string(54,'a')),"capacity rejected");check(!qr.dark[0][0],"failure clears prior symbol");
 if(argc==2){qr.Encode(text);FILE* f=std::fopen(argv[1],"wb");check(f,"QR artifact");int scale=12,size=(29+8)*scale;std::fprintf(f,"P5\n%d %d\n255\n",size,size);for(int y=0;y<size;++y)for(int x=0;x<size;++x){int mx=x/scale-4,my=y/scale-4;std::fputc(mx>=0&&mx<29&&my>=0&&my<29&&qr.dark[my][mx]?0:255,f);}std::fclose(f);}
 std::puts("Bounded local QR encoder passed");}
