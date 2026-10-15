#include "Network/icCompanionLink.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <thread>
static double now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
int main(int argc,char** argv){
    if(argc==5&&std::string(argv[1])=="--send"){
        unsigned port=std::strtoul(argv[3],nullptr,10),code=std::strtoul(argv[4],nullptr,10);if(!port||port>65535)return 2;
        icCompanionLink client;if(!client.Connect(argv[2],(unsigned short)port,code,now()))return 2;double start=now();bool sent=false;
        while(now()-start<6){double t=now();client.Poll(t);if(!sent&&client.Status()==icCompanionLink::State::Connected){icShotIntent fixture;fixture.aim=.25f;fixture.power=.5f;sent=client.SendShot(fixture,t);}
            if(client.Delivered()){std::puts("Synthetic transport fixture acknowledged");return 0;}std::this_thread::sleep_for(std::chrono::milliseconds(4));}
        std::fprintf(stderr,"%s\n",client.Message().c_str());return 1;
    }
    unsigned code=100000+std::random_device{}()%900000,port=0,shots=1;double timeout=120;
    for(int i=1;i<argc;++i){if(i+1>=argc)return 2;std::string key=argv[i++];unsigned value=std::strtoul(argv[i],nullptr,10);if(key=="--port")port=value;else if(key=="--code")code=value;else if(key=="--shots")shots=value;else if(key=="--timeout")timeout=value;else return 2;}
    if(port>65535||timeout<=0||!shots)return 2;
    icCompanionLink host;if(!host.Listen((unsigned short)port,code)){std::fprintf(stderr,"%s\n",host.Message().c_str());return 1;}host.AcceptShots(true);
    std::printf("Companion host address=%s port=%u code=%06u\n",icCompanionLink::LocalAddresses().c_str(),host.Port(),code);std::fflush(stdout);
    double start=now();unsigned received=0;
    while(now()-start<timeout){double t=now();host.Poll(t);icShotIntent shot;
        if(host.TakeShot(shot,t)){std::printf("{\"device\":\"%llu\",\"shot\":%u,\"aim\":%.4f,\"power\":%.4f,\"source\":%u}\n",(unsigned long long)host.PeerDevice(),++received,shot.aim,shot.power,(unsigned)shot.source);std::fflush(stdout);if(received>=shots)return 0;}
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }std::fprintf(stderr,"Timed out waiting for live companion shots\n");return 1;
}
