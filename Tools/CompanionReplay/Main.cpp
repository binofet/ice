#include "Input/icCompanion.h"
#include <iomanip>
#include <iostream>
#include <locale>
int main(int argc,char**argv){
    if(argc!=2){std::cerr<<"Usage: ice_take_replay recording.iceinput\n";return 2;}
    icCompanionSession session;
    if(!session.Take().Load(argv[1])||!session.StartReplay()){std::cerr<<"Invalid or empty companion take\n";return 2;}
    session.AdvanceReplay(301);
    std::cout.imbue(std::locale::classic());std::cout<<std::setprecision(17);
    std::cout<<"{\"matched\":"<<(session.ReplayMatched()?"true":"false")<<",\"shots\":[";
    for(std::size_t i=0;i<session.Shots().size();++i){const auto& shot=session.Shots()[i];if(i)std::cout<<',';
        std::cout<<"{\"source\":\""<<(shot.source==icShotMode::Touch?"touch":"motion")<<"\",\"aim\":"<<shot.aim<<",\"power\":"<<shot.power<<",\"seconds\":"<<shot.seconds<<'}';}
    std::cout<<"]}\n";return session.ReplayMatched()?0:1;
}
