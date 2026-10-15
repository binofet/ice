#include "settings_menu.h"
#include "skeeball.h"
#include "Core/IO/icInput.h"
#include <algorithm>
#include <cstdio>
extern SkeeBall& gApp;
std::vector<ArcadeRect> SettingsMenu::Rows()const{int w,h;gApp.ui.Size(w,h);float width=std::min(560.f,w*.85f),x=(w-width)*.5f,top=h*.57f;return {{x,top,width,64},{x,top-90,width,64},{x,top-196,width,48}};}
void SettingsMenu::OnPush(){m_bRenderTopOnly=true;input.skip=true;icInput::Instance()->ShowCurs();}
ICRESULT SettingsMenu::Update(float,bool*){
    auto keys=icInput::Instance();if(input.skip){input.skip=false;return IC_OK;}
    auto rows=Rows();bool activate=gApp.ui.Navigate(input,rows);
    if(keys->IsPressed(ICKEY_ESCAPE)||(activate&&input.selected==2)){gApp.GetStateMachine().PopState();return IC_OK;}
    auto settings=gApp.GetSettings();float x,y;gApp.ui.Pointer(x,y);
    for(int i=0;i<2;i++){
        float& value=i==0?settings.fSfx_vol:settings.fMusic_vol;
        float begin=rows[i].x+rows[i].w*.45f,end=rows[i].x+rows[i].w-24;
        if(keys->IsDown(ICMOUSE_L)&&rows[i].Contains(x,y)){input.selected=i;value=icClamp((x-begin)/(end-begin),0.f,1.f);}
        if(input.selected==i){if(keys->IsPressed(ICKEY_LEFT))value=std::max(0.f,value-.05f);if(keys->IsPressed(ICKEY_RIGHT))value=std::min(1.f,value+.05f);}
    }
    gApp.UpdateSettings(settings);return IC_OK;
}
ICRESULT SettingsMenu::Render(){
    int w,h;gApp.ui.Size(w,h);auto rows=Rows();auto settings=gApp.GetSettings();gApp.ui.Begin();
    gApp.ui.Heading(rows[0].x,h*.79f,"SETTINGS");gApp.ui.Text(rows[0].x,h*.71f,"Make yourself comfortable.",true);
    const char* labels[]={"SOUND FX","MUSIC"};float values[]={settings.fSfx_vol,settings.fMusic_vol};
    for(int i=0;i<2;i++){
        gApp.ui.Button(rows[i],labels[i],input.selected==i);float begin=rows[i].x+rows[i].w*.45f,width=rows[i].w*.55f-24;
        gApp.ui.Rect({begin,rows[i].y+20,width,6},icColor(50,67,72));gApp.ui.Rect({begin,rows[i].y+20,width*values[i],6},icColor(95,196,181));
        gApp.ui.Rect({begin+width*values[i]-3,rows[i].y+14,6,18},icColor(241,195,119));
        char number[20];std::snprintf(number,sizeof(number),"%d%%",(int)(values[i]*100+.5f));gApp.ui.Text(begin,rows[i].y+40,number,true);
    }
    gApp.ui.Button(rows[2],"BACK",input.selected==2);gApp.ui.Text(rows[0].x,24,"Drag to adjust, or use arrows. Esc: back.",true);gApp.ui.End();return IC_OK;
}
