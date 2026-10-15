#include <cmath>
#include "TestApp.h"
#include "Core/IO/icInput.h"
extern "C" {
#include "Lua/lua.h"
#include "Lua/lauxlib.h"
#include "Lua/lualib.h"
}
TestApp::~TestApp(){Cleanup();}
bool TestApp::ReloadScript(){
    // Use a fresh VM so a failed reload cannot partially change the live scene.
    lua_State* candidate=luaL_newstate();if(!candidate)return false;luaL_openlibs(candidate);
    if(luaL_dofile(candidate,"Resource/scripts/scene.lua")!=LUA_OK){icWarningf("Lua: %s",lua_tostring(candidate,-1));lua_close(candidate);return false;}
    lua_getglobal(candidate,"rotation_speed");bool valid=lua_isnumber(candidate,-1);float speed=(float)lua_tonumber(candidate,-1);
    if(!valid||!std::isfinite(speed)||fabs(speed)>10){lua_close(candidate);icWarning("rotation_speed must be finite and within -10..10");return false;}
    if(m_lua)lua_close(m_lua);m_lua=candidate;m_speed=speed;return true;
}
ICRESULT TestApp::Init(int argc,char** argv){
    icWindowDef def={0,960,640,0,0,"ICE EngineTest",false};
    m_window=icWindow::GetWindow(def);if(!m_window)return IC_FAIL_CREATE_WIND;
    m_pGxDevice=new icGXDeviceGL();ICRESULT result=m_pGxDevice->Init(m_window);if(ICEFAIL(result))return result;
    m_pGxDevice->SetClearColor(icColor(24,32,48));
    m_content=new icContentLoader(m_pGxDevice,nullptr);
    if(ICEFAIL(result=m_content->Load("Resource/models/iceCubeSimple.icm",&m_cube)))return result;
    icFontParams font={"Arial",20};if(ICEFAIL(result=m_pGxDevice->CreateFont(&m_font,&font)))return result;
    m_luaChecks=ReloadScript();if(!m_luaChecks)return IC_FAIL_GEN;
    if(ICEFAIL(m_path.Load("Resource/decent.path")))return IC_FAIL_GEN;
    m_routeChecks=m_path.TryPlotRoute(icVector3(0,0,0),icVector3(99,49,0),m_route);
    if(!m_routeChecks){icWarning("EngineTest route query failed");return IC_FAIL_GEN;}
    icPrintf("EngineTest: Lua scene loaded; path has %d waypoints",m_route._size);
    m_window->Show();m_time.Elapsed();return icApplication::Init(argc,argv);
}
ICRESULT TestApp::Cleanup(){
    if(m_lua){lua_close(m_lua);m_lua=nullptr;}
    if(m_pGxDevice){
        if(m_font){m_pGxDevice->ReleaseFont(m_font);m_font=nullptr;}
        if(m_cube){m_content->Unload(&m_cube);m_cube=nullptr;}
        delete m_content;m_content=nullptr;delete m_pGxDevice;m_pGxDevice=nullptr;
    }
    if(m_window){delete m_window;m_window=nullptr;}return IC_OK;
}
ICRESULT TestApp::Update(){return UpdateFrame(m_time.Elapsed());}
ICRESULT TestApp::UpdateFrame(double seconds){
    auto input=icInput::Instance();float dt=icClamp((float)seconds,0.0f,.1f);
    if(input->IsPressed(ICKEY_ESCAPE))m_window->RequestClose();
    if(input->IsPressed(ICKEY_SPACE))m_paused=!m_paused;
    if(input->IsPressed(ICKEY_R))ReloadScript();
    if(input->IsPressed(ICKEY_TAB))m_showRoute=!m_showRoute;
    if(!m_paused)m_angle+=dt*m_speed;
    input->Update();return IC_OK;
}
ICRESULT TestApp::RenderFrame(bool present){
    int width,height;m_window->GetClientSize(&width,&height);if(height<=0)return IC_OK;
    m_pGxDevice->FrameBegin();icMatrix44 projection,view,world;
    icCreatePerspective(IC_PI/4,(icReal)width/height,1,1000,projection);
    icCreateLookAt(icVector3(0,12,-45),icVector3::ZERO,icVector3::Y_AXIS,view);
    m_pGxDevice->SetProjection(projection);m_pGxDevice->SetViewMatrix(view);
    world.Identity();world.RotY(m_angle);world.Scale(8,8,8);ICRESULT result=m_cube->Render(world);
    m_font->Print(icVector2Int(20,height-35),icColor::WHITE,"EngineTest | Space: pause | R: reload Lua | Tab: route | Esc: close");
    m_font->Printf(icVector2Int(20,25),icColor::WHITE,"Lua speed %.2f | Path check: %d waypoints",m_speed,m_route._size);
    if(m_showRoute){
        m_pGxDevice->SetProjection(projection);m_pGxDevice->SetViewMatrix(view);world.Identity();m_pGxDevice->SetWorldTransform(world);
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LINE_BIT);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_DEPTH_TEST);glLineWidth(2);glColor3f(.2f,1,.4f);
        glBegin(GL_LINE_STRIP);for(int i=0;i<m_route._size;++i)glVertex3f((m_route._wayPoints[i].x-50)*.2f,0,(m_route._wayPoints[i].y-25)*.2f);glEnd();glPopAttrib();
    }
    if(present)PresentFrame();return result;
}
