#include "TestApp.h"
#include "SampleHost.h"
int main(int argc,char** argv){
    sample::initialize();TestApp app;bool smoke=argc==3&&std::string(argv[1])=="--smoke-test";
    if(ICEFAIL(app.Init(argc,argv)))return 1;
    unsigned frames=0;ICRESULT result=IC_OK;
    while(!app.Window()->ShouldClose()){
        icWindow::PumpEvents();if(app.Window()->ShouldClose())break;icPlatformEvent event;while(icPlatformEvents::Poll(event)){}
        result=app.Update();if(ICEFAIL(result))break;
        bool capture=smoke&&frames==90;result=app.RenderFrame(!capture);if(ICEFAIL(result))break;
        if(smoke&&glGetError()!=GL_NO_ERROR){result=IC_FAIL_GEN;break;}
        if(capture){if(!sample::capture(app.Window(),std::string(argv[2])+"/engine-test.ppm")){result=IC_FAIL_GEN;break;}app.PresentFrame();}
        ++frames;if(smoke&&frames==120){icPrintf("EngineTest smoke passed: %u frames; Lua and path checks passed",frames);break;}
        if(smoke)std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    return ICEFAIL(result)||!app.ChecksPassed()?1:0;
}
