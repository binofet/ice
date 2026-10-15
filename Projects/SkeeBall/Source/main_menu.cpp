#include "main_menu.h"
#include "skeeball.h"
#include "Core/IO/icInput.h"
#include <algorithm>
extern SkeeBall& gApp;
std::vector<ArcadeRect> MainMenu::Buttons() const{int w,h;gApp.ui.Size(w,h);float step=h<600?48:58,height=step-10;std::vector<ArcadeRect> boxes;for(int i=0;i<4;i++)boxes.push_back({w*.075f,h*.48f-i*step-height,std::min(360.f,w*.8f),height});return boxes;}
ICRESULT MainMenu::Update(float,bool*){
    if(SkipUpdate){SkipUpdate=false;return IC_OK;}
    if(icInput::Instance()->IsPressed(ICKEY_ESCAPE)){gApp.Quit();return IC_OK;}
    if(gApp.ui.Navigate(input,Buttons())){
        if(input.selected==0)gApp.GetStateMachine().PushState(&gApp.m_GameState);
        if(input.selected==1){gApp.ToggleBrowser();gApp.GetStateMachine().PushState(&gApp.m_GameState);}
        if(input.selected==2)gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);
        if(input.selected==3)gApp.Quit();
    }return IC_OK;
}
ICRESULT MainMenu::Render(){
    int w,h;gApp.ui.Size(w,h);float x=w*.075f;gApp.ui.Begin();
    gApp.ui.Heading(x,h*.77f,"SKEEBALL");gApp.ui.Text(x,h*.68f,"THE ICE ARCADE");gApp.ui.Text(x,h*.59f,"Good company. One more roll.");
    auto boxes=Buttons();const char* labels[]={"PLAY SOLO","PHONE PARTY","SETTINGS","QUIT"};for(int i=0;i<4;i++)gApp.ui.Button(boxes[i],labels[i],input.selected==i);
    gApp.ui.Text(x,24,"Phones welcome. No app needed.",true);gApp.ui.End();return IC_OK;
}
void MainMenu::OnPush(){m_bRenderTopOnly=true;input.skip=true;icInput::Instance()->ShowCurs();}
