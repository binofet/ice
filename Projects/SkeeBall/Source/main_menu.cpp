#include "main_menu.h"
#include "skeeball.h"
#include "Core/IO/icInput.h"
#include "Core/GXDevice/icGLext.h"
extern SkeeBall& gApp;
ICRESULT MainMenu::Init(icContentLoader* loader){
    content=loader;if(!content)return IC_FAIL_GEN;
    if(ICEFAIL(content->Load("Resource/textures/title.tga",&background)))return IC_FAIL_GEN;
    title.Initialize("Arial",48,content->GetDevice());body.Initialize("Arial",24,content->GetDevice());return IC_OK;
}
ICRESULT MainMenu::Update(float,bool*){
    auto input=icInput::Instance();if(SkipUpdate){SkipUpdate=false;return IC_OK;}
    if(input->IsPressed(ICKEY_DOWN))selected=(selected+1)%4;
    if(input->IsPressed(ICKEY_UP))selected=(selected+3)%4;
    short x,y;input->GetPos(&x,&y);float w=gApp.GetSettings().iScreen_x,h=gApp.GetSettings().iScreen_y;
    int hover=-1;for(int i=0;i<4;i++){float top=h*.48f-i*58;if(x>=w*.075f&&x<=w*.075f+310&&y<=top&&y>=top-46)hover=i;}
    if((x!=lastX||y!=lastY)&&hover>=0)selected=hover;lastX=x;lastY=y;
    bool activate=input->IsPressed(ICKEY_RETURN)||(hover>=0&&input->IsPressed(ICMOUSE_L));
    if(input->IsPressed(ICKEY_ESCAPE)){gApp.Quit();return IC_OK;}
    if(activate){
        if(hover>=0&&input->IsPressed(ICMOUSE_L))selected=hover;
        if(selected==0)gApp.GetStateMachine().PushState(&gApp.m_GameState);
        if(selected==1){gApp.ToggleBrowser();gApp.GetStateMachine().PushState(&gApp.m_GameState);}
        if(selected==2){m_bRenderTopOnly=false;gApp.GetStateMachine().PushState(&gApp.m_SettingsMenu);}
        if(selected==3)gApp.Quit();
    }
    return IC_OK;
}
ICRESULT MainMenu::Render(){
    auto dev=content->GetDevice();icMatrix44 identity;identity.Identity();dev->SetProjection(dev->GetOrtho());dev->SetViewMatrix(identity);dev->SetWorldTransform(identity);
    float w=gApp.GetSettings().iScreen_x,h=gApp.GetSettings().iScreen_y,x=w*.075f;
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glColor4f(1,1,1,1);
    dev->SetTexture(0,background);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(w,0);glTexCoord2f(1,1);glVertex2f(w,h);glTexCoord2f(0,1);glVertex2f(0,h);glEnd();
    glDisable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);glColor4f(.015f,.03f,.045f,.85f);glVertex2f(0,0);glVertex2f(0,h);glColor4f(.015f,.03f,.045f,0);glVertex2f(w*.63f,h);glVertex2f(w*.63f,0);glEnd();
    const char* labels[]={"PLAY SOLO","PHONE PARTY","SETTINGS","QUIT"};
    for(int i=0;i<4;i++){float top=h*.48f-i*58;bool active=i==selected;glColor4f(active?.09f:.025f,active?.36f:.07f,active?.38f:.09f,.92f);glBegin(GL_QUADS);glVertex2f(x,top);glVertex2f(x+310,top);glVertex2f(x+310,top-46);glVertex2f(x,top-46);glEnd();if(active){glColor3f(.95f,.67f,.31f);glBegin(GL_QUADS);glVertex2f(x,top);glVertex2f(x+4,top);glVertex2f(x+4,top-46);glVertex2f(x,top-46);glEnd();}}
    glPopAttrib();
    title.Print(icVector2Int(x,h*.77f),icColor(244,224,187,255),"SKEEBALL");
    body.Print(icVector2Int(x,h*.68f),icColor(103,203,198,255),"THE ICE ARCADE");
    body.Print(icVector2Int(x,h*.59f),icColor(225,221,208,255),"Good company. One more roll.");
    for(int i=0;i<4;i++)body.Print(icVector2Int(x+22,h*.48f-i*58-34),icColor(241,229,207,255),labels[i]);
    body.Print(icVector2Int(x,28),icColor(188,204,205,255),"Phones welcome. No app needed.");
    return IC_OK;
}
void MainMenu::OnPush(){m_bRenderTopOnly=true;SkipUpdate=true;icInput::Instance()->ShowCurs();}
void MainMenu::OnPop(){m_bRenderTopOnly=true;}
