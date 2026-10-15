#include "pause_menu.h"
#include "skeeball.h"
#include "Core/IO/icInput.h"
#include <algorithm>
extern SkeeBall& gApp;
std::vector<ArcadeRect> PauseMenu::Buttons()const{int w,h;gApp.ui.Size(w,h);float width=std::min(420.f,w*.85f),x=(w-width)*.5f;std::vector<ArcadeRect> result;for(int i=0;i<3;i++)result.push_back({x,h*.56f-i*66, width,48});return result;}
void PauseMenu::OnPush(){m_bRenderTopOnly=true;input.skip=true;icInput::Instance()->ShowCurs();}
ICRESULT PauseMenu::Update(float,bool*){
    if(SkipUpdate){SkipUpdate=false;return IC_OK;}auto keys=icInput::Instance();if(input.skip){input.skip=false;return IC_OK;}
    bool activate=gApp.ui.Navigate(input,Buttons());
    if(keys->IsPressed(ICKEY_ESCAPE)||(activate&&input.selected==0)){gApp.GetStateMachine().PopState();icInput::Instance()->HideCurs();return IC_OK;}
    if(activate&&input.selected==1)gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);
    if(activate&&input.selected==2){while(gApp.GetStateMachine().PeekState()&&gApp.GetStateMachine().PeekState()!=&gApp.m_MainMenu)gApp.GetStateMachine().PopState();gApp.m_MainMenu.OnPush();}
    return IC_OK;
}
ICRESULT PauseMenu::Render(){int w,h;gApp.ui.Size(w,h);auto boxes=Buttons();gApp.ui.Begin();gApp.ui.Heading(boxes[0].x,h*.76f,"TAKE A BREATHER");const char* labels[]={"RESUME","SETTINGS","TITLE SCREEN"};for(int i=0;i<3;i++)gApp.ui.Button(boxes[i],labels[i],input.selected==i);gApp.ui.Text(boxes[0].x,24,"Esc: back to the lane.",true);gApp.ui.End();return IC_OK;}
