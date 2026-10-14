#ifndef ICE_TEST_APP_H
#define ICE_TEST_APP_H
#include "Core/Application/icApplication.h"
#include "Core/GXDevice/icGXDeviceGL.h"
#include "Content/icContentLoader.h"
#include "Render/icModel.h"
#include "Render/Font/icFont.h"
#include "AI/Pathing/icAStar.h"
#include "Util/icTime.h"
struct lua_State;
class TestApp : public icApplication {
public:
    ~TestApp() override;
    ICRESULT Init(int argc,char** argv) override;
    ICRESULT Cleanup() override;
    ICRESULT Update() override;
    ICRESULT Render() override {return RenderFrame(true);}
    ICRESULT RenderFrame(bool present);
    void PresentFrame(){m_pGxDevice->FrameEnd(true);}
    icWindow* Window() const {return m_window;}
    bool ChecksPassed() const {return m_luaChecks&&m_routeChecks;}
private:
    bool ReloadScript();
    icWindow* m_window=nullptr;
    icContentLoader* m_content=nullptr;
    icModel* m_cube=nullptr;
    icFontI* m_font=nullptr;
    lua_State* m_lua=nullptr;
    icAStar m_path;
    icRouteBuffer m_route={};
    icTimer m_time;
    float m_angle=0,m_speed=.6f;
    bool m_paused=false,m_showRoute=false,m_luaChecks=false,m_routeChecks=false;
};
#endif
