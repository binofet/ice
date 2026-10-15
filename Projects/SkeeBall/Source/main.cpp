#include "skeeball.h"
#include "SampleHost.h"
#include "Core/IO/icInput.h"
SkeeBall app;
SkeeBall& gApp(app);

// Exercise actual surface changes in an isolated sample run, without persisted settings edits.
static ICRESULT layoutSmoke(const std::string& directory){
    struct Phase {const char* name;short width,height;int state;bool fullscreen;};
    const Phase phases[]={
        {"title-window",1280,720,0,false},{"title-ultrawide",2560,1080,0,false},
        {"settings-ultrawide",2560,1080,1,false},{"title-compact",640,480,0,false},
        {"settings-compact",640,480,1,false},{"game-compact",640,480,2,false},
        {"pause-compact",640,480,3,false},{"game-ultrawide",2560,1080,2,false},
        {"title-fullscreen",1280,720,0,true},{"settings-fullscreen",1280,720,1,true},
        {"pause-fullscreen",1280,720,3,true},{"title-restored",1280,720,0,false}};
    for(const auto& phase:phases){
        while(app.GetStateMachine().PeekState()!=&app.m_MainMenu){if(!app.GetStateMachine().PopState())return IC_FAIL_GEN;}
        icInput::Instance()->CancelHeldInput();app.m_MainMenu.OnPush();
        icWindowDef def;app.GetMainWindow()->GetParams(&def);def.i16Width=phase.width;def.i16Height=phase.height;def.bFullScreen=phase.fullscreen;
        if(ICEFAIL(app.GetMainWindow()->UpdateParams(def)))return IC_FAIL_GEN;
        if(phase.state==1)app.GetStateMachine().PushState(&app.m_SettingsMenu);
        if(phase.state>=2)app.GetStateMachine().PushState(&app.m_GameState);
        if(phase.state==3)app.GetStateMachine().PushState(&app.m_PauseMenu);
        for(int frame=0;frame<10;frame++){
            icWindow::PumpEvents();if(ICEFAIL(app.Update()))return IC_FAIL_GEN;
            if(ICEFAIL(app.RenderFrame(frame!=9))||glGetError()!=GL_NO_ERROR)return IC_FAIL_GEN;
            if(frame==9){if(!sample::capture(app.GetMainWindow(),directory+"/"+phase.name+".ppm"))return IC_FAIL_GEN;app.PresentFrame();}
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        int w,h;app.GetMainWindow()->GetFramebufferSize(&w,&h);icPrintf("Layout checked: %s %dx%d",phase.name,w,h);
        auto key=[](uchar code){icInput::Instance()->SetPressed(code,true);return app.Update();};
        if(std::string(phase.name)=="title-ultrawide"){
            // Synthetic input fixture: use client coordinates as native input does.
            int cw,ch;app.GetMainWindow()->GetClientSize(&cw,&ch);
            float px=w*.075f+100,py=h*.48f-2*58-24;
            icInput::Instance()->UpdateMousePosition(px*cw/w,ch-py*ch/h,ch);
            if(ICEFAIL(key(ICMOUSE_L))||app.GetStateMachine().PeekState()!=&app.m_SettingsMenu)return IC_FAIL_GEN;
            app.Update();if(ICEFAIL(key(ICKEY_ESCAPE))||app.GetStateMachine().PeekState()!=&app.m_MainMenu)return IC_FAIL_GEN;
            icPrintf("UI input checked: resized mouse hit-test, settings and back");
        }
        if(std::string(phase.name)=="game-compact"){
            icInput::Instance()->SetDown(ICKEY_SPACE,true);
            for(int i=0;i<12;i++){app.Update();std::this_thread::sleep_for(std::chrono::milliseconds(8));}
            icInput::Instance()->SetDown(ICKEY_SPACE,false);app.Update();
            if(!app.m_GameState.ShotInFlight())return IC_FAIL_GEN;
            if(ICEFAIL(key(ICKEY_ESCAPE))||app.GetStateMachine().PeekState()!=&app.m_PauseMenu)return IC_FAIL_GEN;
            app.Update();key(ICKEY_DOWN);key(ICKEY_RETURN);
            if(app.GetStateMachine().PeekState()!=&app.m_SettingsMenu)return IC_FAIL_GEN;
            app.Update();auto settings=app.GetSettings();key(ICKEY_RIGHT);
            if(app.GetSettings().fSfx_vol!=std::min(1.f,settings.fSfx_vol+.05f))return IC_FAIL_GEN;
            app.UpdateSettings(settings);key(ICKEY_ESCAPE);
            if(app.GetStateMachine().PeekState()!=&app.m_PauseMenu)return IC_FAIL_GEN;
            key(ICKEY_ESCAPE);
            if(app.GetStateMachine().PeekState()!=&app.m_GameState||!app.m_GameState.ShotInFlight())return IC_FAIL_GEN;
            icPrintf("UI input checked: volume, settings/back/resume preserve an in-flight shot");
        }
    }
    return IC_OK;
}

int main(int argc,char** argv){
    sample::initialize();bool smoke=argc==3&&std::string(argv[1])=="--smoke-test";
    ICRESULT result=app.Init(argc,argv);if(ICEFAIL(result)){icWarning("SkeeBall initialization failed");return 1;}
    if(argc==3&&std::string(argv[1])=="--layout-smoke"){result=layoutSmoke(argv[2]);app.Cleanup();return ICEFAIL(result)?1:0;}
    for(int i=1;i<argc;++i)if(std::string(argv[i])=="--companion"){app.ToggleCompanion();app.GetStateMachine().PushState(&app.m_GameState);}
    for(int i=1;i<argc;++i)if(std::string(argv[i])=="--browser-controller"){app.ToggleBrowser();app.GetStateMachine().PushState(&app.m_GameState);}
    bool pairingSmoke=argc==3&&std::string(argv[1])=="--pairing-smoke";
    bool partySmoke=argc==3&&std::string(argv[1])=="--party-smoke";
    if(pairingSmoke)app.ToggleCompanion();
    if(partySmoke)app.ToggleBrowser();
    if(smoke&&!std::getenv("ICE_DISABLE_SHADERS")){
        icProgramGL program;std::string log;
        if(!program.Compile("#version 120\nvoid main(){gl_Position=gl_Vertex;}","#version 120\nvoid main(){gl_FragColor=vec4(1.0);}",log))return 2;
        auto original=program.Name();
        if(program.Compile("invalid source","invalid source",log)||program.Name()!=original||log.empty())return 3;
        icPrintf("GLSL smoke passed: valid link, diagnostic failure, transactional replacement");
    }
    unsigned frame=0;bool launched=false;
    while(!app.GetMainWindow()->ShouldClose()){
        icWindow::PumpEvents();if(app.GetMainWindow()->ShouldClose())break;
        icPlatformEvent event;while(icPlatformEvents::Poll(event)){
            if(event.window==app.GetMainWindow()->GetID()){
                if(event.type==icPlatformEventType::FocusLost)app.CompanionFocused=false;
                if(event.type==icPlatformEventType::FocusGained)app.CompanionFocused=true;
            }
        }
        if(smoke&&frame==30)app.GetStateMachine().PushState(&app.m_GameState);
        if(smoke)icInput::Instance()->SetDown(ICKEY_SPACE,frame>=50&&frame<200);
        result=app.Update();if(ICEFAIL(result))break;
        if(smoke&&frame==201)launched=app.m_GameState.ShotInFlight();
        bool panelCapture=(pairingSmoke||partySmoke)&&frame==30;
        bool capture=panelCapture||(smoke&&(frame==20||frame==240));
        result=app.RenderFrame(!capture);if(ICEFAIL(result))break;
        if(smoke&&glGetError()!=GL_NO_ERROR){icWarningf("SkeeBall GL error at frame %u",frame);result=IC_FAIL_GEN;break;}
        if(capture){if(!sample::capture(app.GetMainWindow(),panelCapture?std::string(argv[2]):std::string(argv[2])+(frame==20?"/skeeball-menu.ppm":"/skeeball-game.ppm"))){result=IC_FAIL_GEN;break;}app.PresentFrame();if(panelCapture)break;}
        ++frame;if(smoke&&frame==300){if(!launched){icWarning("Smoke shot did not launch");result=IC_FAIL_GEN;break;}icPrintf("SkeeBall smoke passed: %u frames",frame);break;}
        if(smoke)std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    app.Cleanup();return ICEFAIL(result)?1:0;
}
