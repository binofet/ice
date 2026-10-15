#include "arcade_ui.h"
#include "skeeball.h"
#include "Core/IO/icInput.h"
#include <algorithm>
extern SkeeBall& gApp;
ICRESULT ArcadeUI::Init(icContentLoader* loader){
    content=loader;ICRESULT r=content->Load("Resource/textures/title.tga",&background);
    r|=title.Initialize("Arial",48,content->GetDevice());r|=body.Initialize("Arial",24,content->GetDevice());r|=small.Initialize("Arial",18,content->GetDevice());return r;
}
void ArcadeUI::Size(int& w,int& h) const{gApp.GetMainWindow()->GetFramebufferSize(&w,&h);}
void ArcadeUI::Pointer(float& x,float& y) const{short px,py;icInput::Instance()->GetPos(&px,&py);int w,h,cw,ch;Size(w,h);gApp.GetMainWindow()->GetClientSize(&cw,&ch);x=cw>0?float(px)*w/cw:0;y=ch>0?float(py)*h/ch:0;}
void ArcadeUI::Begin(bool drawBackground){
    auto dev=content->GetDevice();icMatrix44 identity;identity.Identity();dev->SetProjection(dev->GetOrtho());dev->SetViewMatrix(identity);dev->SetWorldTransform(identity);
    glPushAttrib(GL_ALL_ATTRIB_BITS);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);
    if(!drawBackground)return;
    int w,h;Size(w,h);if(w<=0||h<=0)return;
    // Cover the live surface without stretching the original 16:9 artwork.
    float u0=0,v0=0,u1=1,v1=1,aspect=float(w)/h,artAspect=16.f/9;
    if(aspect>artAspect){float visible=artAspect/aspect;v0=(1-visible)*.5f;v1=1-v0;}
    else {float visible=aspect/artAspect;u0=(1-visible)*.5f;u1=1-u0;}
    dev->SetTexture(0,background);glDisable(GL_BLEND);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor4f(1,1,1,1);
    glBegin(GL_QUADS);glTexCoord2f(u0,v0);glVertex2f(0,0);glTexCoord2f(u1,v0);glVertex2f(w,0);glTexCoord2f(u1,v1);glVertex2f(w,h);glTexCoord2f(u0,v1);glVertex2f(0,h);glEnd();
    glDisable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);glColor4f(.015f,.03f,.045f,.94f);glVertex2f(0,0);glVertex2f(0,h);glColor4f(.015f,.03f,.045f,.1f);glVertex2f(w,h);glVertex2f(w,0);glEnd();
}
void ArcadeUI::End(){glPopAttrib();}
void ArcadeUI::Rect(const ArcadeRect& r,const icColor& color){glDisable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(color.R(),color.G(),color.B(),color.A());glBegin(GL_QUADS);glVertex2f(r.x,r.y);glVertex2f(r.x+r.w,r.y);glVertex2f(r.x+r.w,r.y+r.h);glVertex2f(r.x,r.y+r.h);glEnd();}
void ArcadeUI::Text(float x,float y,const char* text,bool compact){(compact?small:body).Print(icVector2Int(x,y),icColor(235,230,213),text);}
void ArcadeUI::Heading(float x,float y,const char* text){title.Print(icVector2Int(x,y),icColor(244,224,187),text);}
void ArcadeUI::Button(const ArcadeRect& r,const char* text,bool selected){Rect(r,selected?icColor(24,92,96,242):icColor(7,25,32,235));if(selected)Rect({r.x,r.y,4,r.h},icColor(242,171,79));Text(r.x+22,r.y+(r.h-24)*.5f+2,text);}
bool ArcadeUI::Navigate(ArcadeMenuInput& state,const std::vector<ArcadeRect>& boxes){
    if(state.skip){state.skip=false;return false;}auto input=icInput::Instance();int count=(int)boxes.size();if(!count)return false;
    if(input->IsPressed(ICKEY_DOWN))state.selected=(state.selected+1)%count;
    if(input->IsPressed(ICKEY_UP))state.selected=(state.selected+count-1)%count;
    float x,y;Pointer(x,y);int hover=-1;for(int i=0;i<count;i++)if(boxes[i].Contains(x,y))hover=i;
    if(hover>=0&&(x!=state.lastX||y!=state.lastY||input->IsPressed(ICMOUSE_L)))state.selected=hover;
    state.lastX=x;state.lastY=y;return input->IsPressed(ICKEY_RETURN)||(hover>=0&&input->IsPressed(ICMOUSE_L));
}
