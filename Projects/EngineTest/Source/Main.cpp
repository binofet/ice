#include "TestApp.h"
#include "SampleHost.h"
#include "Platform/icHost.h"
struct DesktopClient : icHostClient {
    TestApp& app;
    bool smoke;
    std::string directory;
    unsigned frames=0;
    ICRESULT result=IC_OK;
    DesktopClient(TestApp& a,bool s,const std::string& d):app(a),smoke(s),directory(d){}
    void OnUpdate(double dt) override {result=app.UpdateFrame(dt);}
    void OnRender(const icSurface&) override {
        if(ICEFAIL(result))return;
        bool capture=smoke&&frames==90;result=app.RenderFrame(!capture);
        if(ICEFAIL(result))return;
        if(smoke&&glGetError()!=GL_NO_ERROR){result=IC_FAIL_GEN;return;}
        if(capture){if(!sample::capture(app.Window(),directory+"/engine-test.ppm"))result=IC_FAIL_GEN;app.PresentFrame();}
        ++frames;
    }
};
int main(int argc,char** argv){
    sample::initialize();TestApp app;bool smoke=argc==3&&std::string(argv[1])=="--smoke-test";
    if(ICEFAIL(app.Init(argc,argv)))return 1;
    DesktopClient client(app,smoke,smoke?argv[2]:"");icHost host(client);
    int width,height;app.Window()->GetFramebufferSize(&width,&height);
    icSurface surface;surface.api=icSurfaceAPI::OpenGL;surface.width=width;surface.height=height;
    auto id=host.AddSurface(surface);host.SetState(icAppState::Active);
    bool exercised=false;
    while(!app.Window()->ShouldClose()){
        icWindow::PumpEvents();if(app.Window()->ShouldClose())break;
        icPlatformEvent event;
        while(icPlatformEvents::Poll(event)){
            if(!smoke&&event.window==app.Window()->GetID()){
                if(event.type==icPlatformEventType::FocusLost)host.SetState(icAppState::Inactive);
                if(event.type==icPlatformEventType::FocusGained)host.SetState(icAppState::Active);
            }
        }
        app.Window()->GetFramebufferSize(&width,&height);host.ResizeSurface(id,width,height,1);
        double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if(smoke&&!exercised&&client.frames==60){
            host.SetState(icAppState::Background);unsigned previous=client.frames;host.Frame(now+30);
            if(client.frames!=previous){client.result=IC_FAIL_GEN;break;}
            host.RemoveSurface(id);id=host.AddSurface(surface);host.SetState(icAppState::Active);exercised=true;
        }
        host.Frame(now);if(ICEFAIL(client.result))break;
        if(smoke&&client.frames==120){icPrintf("EngineTest smoke passed: %u frames; Lua, path, host pause and surface recreation checks passed",client.frames);break;}
        if(smoke||host.State()!=icAppState::Active)std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    host.Stop();return ICEFAIL(client.result)||!app.ChecksPassed()?1:0;
}
