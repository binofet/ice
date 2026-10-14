#include "airhockey.h"
#include "Core/Application/Window/icWindow.h"
#include "Core/IO/icInput.h"
#include "Math/icRand.h"
#include "Memory/icMemory.h"
#include "Util/icTime.h"
#include "Core/GXDevice/icGLext.h"
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <limits.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
static bool capture(icWindow* window,const std::string& path) {
    int width,height;window->GetFramebufferSize(&width,&height);
    std::vector<unsigned char> pixels(width*height*3);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    if(std::count_if(pixels.begin(),pixels.end(),[](unsigned char value){return value>20;})<128)return false;
    FILE* file=fopen(path.c_str(),"wb");if(!file)return false;
    fprintf(file,"P6\n%d %d\n255\n",width,height);
    for(int row=height-1;row>=0;--row)fwrite(pixels.data()+row*width*3,1,width*3,file);
    fclose(file);return glGetError()==GL_NO_ERROR;
}
struct GameplaySmokeChecks {
    static bool run(GameState& game) {
        auto puck=game.m_Puck.GetPhys();
        puck->SetXForm(b2Vec2(0,.9f),0);game.UpdateScore();
        if(game.GetPlayer1Score()!=1)return false;
        game.UpdateScore();if(game.GetPlayer1Score()!=1)return false;
        game.SetPlayer1Score(WINING_SCORE-1);
        puck->SetXForm(b2Vec2(0,.9f),0);game.UpdateScore();
        if(game.GetPlayer1Score()!=WINING_SCORE)return false;
        game.UpdateScore();if(game.GetPlayer1Score()!=WINING_SCORE)return false;
        game.OnPush();
        return game.GetPlayer1Score()==0&&game.GetPlayer2Score()==0;
    }
};
Airhockey app;
Airhockey& gApp(app);
int main(int argc,char* args[]) {
    setvbuf(stdout,nullptr,_IONBF,0);
    char executable[4096]={};
#ifdef __APPLE__
    uint32_t size=sizeof(executable);bool located=_NSGetExecutablePath(executable,&size)==0;
#elif defined(WIN32)
    bool located=GetModuleFileNameA(nullptr,executable,sizeof(executable))>0;
#else
    ssize_t length=readlink("/proc/self/exe",executable,sizeof(executable)-1);bool located=length>0;
#endif
    if(located){
        std::string path(executable);auto separator=path.find_last_of("/\\");
#ifdef WIN32
        SetCurrentDirectoryA(path.substr(0,separator).c_str());
#else
        chdir(path.substr(0,separator).c_str());
#endif
    }
    icMemory::Init();icSysTimeInit();icRandAutoSeed();
    bool smoke=argc==3&&std::string(args[1])=="--smoke-test";
    std::string output=smoke?args[2]:"";
    ICRESULT result=app.Init(argc,args);
    if(ICEFAIL(result)){icWarningf("Airhockey could not start: %s",icGetStr(result));return 1;}
    unsigned frame=0,pausedSteps=0;
    while(auto window=app.GetMainWindow()){
        if(window->ShouldClose())break;
        icWindow::PumpEvents();
        // Airhockey has one game window; future tools route these events by ID.
        icPlatformEvent platformEvent;
        while(icPlatformEvents::Poll(platformEvent)) {
            if(platformEvent.type==icPlatformEventType::ApplicationQuitRequested)app.Quit();
        }
        if(window->ShouldClose())break;
        if(smoke&&frame==30){app.m_GameState.GameMode=GAME_MODE_SINGLE;app.GetStateMachine().PushState(&app.m_GameState);
            if(!GameplaySmokeChecks::run(app.m_GameState)){icWarning("Scoring smoke check failed");result=IC_FAIL_GEN;break;}
        }
        if(smoke&&frame==300){pausedSteps=app.m_GameState.PhysicsSteps();app.GetStateMachine().PushState(&app.m_PauseMenu);}
        if(smoke&&frame==330){
            if(app.m_GameState.PhysicsSteps()!=pausedSteps){result=IC_FAIL_GEN;break;}
            app.GetStateMachine().PopState();
        }
        if(smoke)std::this_thread::sleep_for(std::chrono::milliseconds(8));
        result=app.Update();
        if(ICEFAIL(result))break;
        bool captureFrame=smoke&&(frame==20||frame==290||frame==320);
        result=app.RenderFrame(!captureFrame);
        if(ICEFAIL(result))break;
        GLenum error=glGetError();
        if(smoke&&error!=GL_NO_ERROR){icWarningf("GL error at frame %u: 0x%x",frame,error);result=IC_FAIL_GEN;break;}
        if(captureFrame){
            std::string name=frame==20?"menu":frame==290?"game":"pause";
            if(!capture(window,output+"/"+name+".ppm")){icWarning("Empty or invalid framebuffer capture");result=IC_FAIL_GEN;break;}
            app.PresentFrame();
        }
        ++frame;
        if(smoke&&frame==390){
            icPrintf("SMOKE frames=%u physics_steps=%u",frame,app.m_GameState.PhysicsSteps());
            if(app.m_GameState.PhysicsSteps()<30||app.m_GameState.PhysicsSteps()<=pausedSteps)result=IC_FAIL_GEN;
            break;
        }
    }
    app.Cleanup();
    return ICEFAIL(result)?1:0;
}
