#include "skeeball.h"
#include "SampleHost.h"
#include "Core/IO/icInput.h"
SkeeBall app;
SkeeBall& gApp(app);
int main(int argc,char** argv){
    sample::initialize();bool smoke=argc==3&&std::string(argv[1])=="--smoke-test";
    ICRESULT result=app.Init(argc,argv);if(ICEFAIL(result)){icWarning("SkeeBall initialization failed");return 1;}
    unsigned frame=0;bool launched=false;
    while(!app.GetMainWindow()->ShouldClose()){
        icWindow::PumpEvents();if(app.GetMainWindow()->ShouldClose())break;
        icPlatformEvent event;while(icPlatformEvents::Poll(event)){}
        if(smoke&&frame==30)app.GetStateMachine().PushState(&app.m_GameState);
        if(smoke)icInput::Instance()->SetDown(ICKEY_SPACE,frame>=50&&frame<200);
        result=app.Update();if(ICEFAIL(result))break;
        if(smoke&&frame==201)launched=app.m_GameState.ShotInFlight();
        bool capture=smoke&&(frame==20||frame==240);
        result=app.RenderFrame(!capture);if(ICEFAIL(result))break;
        if(smoke&&glGetError()!=GL_NO_ERROR){icWarningf("SkeeBall GL error at frame %u",frame);result=IC_FAIL_GEN;break;}
        if(capture){if(!sample::capture(app.GetMainWindow(),std::string(argv[2])+(frame==20?"/skeeball-menu.ppm":"/skeeball-game.ppm"))){result=IC_FAIL_GEN;break;}app.PresentFrame();}
        ++frame;if(smoke&&frame==300){if(!launched){icWarning("Smoke shot did not launch");result=IC_FAIL_GEN;break;}icPrintf("SkeeBall smoke passed: %u frames",frame);break;}
        if(smoke)std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    app.Cleanup();return ICEFAIL(result)?1:0;
}
