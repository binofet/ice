#include "Core/Application/Window/icWindowX11.h"
#include "Core/IO/icInput.h"
#include "Core/GXDevice/icGLext.h"
#include <X11/keysym.h>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"X11 smoke failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static bool draw(icWindowX11& window,float red,float green,float blue){
    window.MakeCurrent();int width,height;window.GetFramebufferSize(&width,&height);
    glViewport(0,0,width,height);glClearColor(red,green,blue,1);glClear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4]={};glReadBuffer(GL_BACK);glReadPixels(width/2,height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    bool correct=abs(pixel[0]-(int)(red*255))<3&&abs(pixel[1]-(int)(green*255))<3&&abs(pixel[2]-(int)(blue*255))<3;
    window.SwapBuffers();return correct&&glGetError()==GL_NO_ERROR;
}
int main(){
    icWindowDef def={};def.i16Width=320;def.i16Height=240;def.pWindowName="ICE native main";
    icWindowX11 main(def);CHECK(main.GetRef());CHECK(main.CreateGLContext());CHECK(!ICEFAIL(main.Show()));
    def.i16Width=200;def.i16Height=160;def.pWindowName="ICE native preview";
    icWindowX11 preview(def,&main);CHECK(preview.GetRef());CHECK(preview.CreateGLContext());CHECK(!ICEFAIL(preview.Show()));CHECK(main.GetID()!=preview.GetID()&&main.GetID()!=0);
    XSync(main.GetDisplay(),False);icWindow::PumpEvents();
    ::Window parent=0;CHECK(XGetTransientForHint(main.GetDisplay(),preview.GetRef(),&parent)&&parent==main.GetRef());
    CHECK(draw(main,1,0,0));CHECK(draw(preview,0,1,0));CHECK(draw(main,0,0,1));
    def.i16Width=260;def.i16Height=180;def.pWindowName="ICE resized preview";
    CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    int width,height;preview.GetClientSize(&width,&height);CHECK(width==260&&height==180);CHECK(draw(preview,1,1,0));
    def.bFullScreen=true;CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    preview.GetClientSize(&width,&height);CHECK(width>0&&height>0);
    def.bFullScreen=false;CHECK(!ICEFAIL(preview.UpdateParams(def)));icWindow::PumpEvents();
    preview.GetClientSize(&width,&height);CHECK(width==260&&height==180);
    XEvent event={};event.xkey.type=KeyPress;event.xkey.display=main.GetDisplay();event.xkey.window=preview.GetRef();event.xkey.keycode=XKeysymToKeycode(main.GetDisplay(),XK_a);
    XSendEvent(main.GetDisplay(),preview.GetRef(),False,KeyPressMask,&event);XSync(main.GetDisplay(),False);icWindow::PumpEvents();
    CHECK(icInput::Instance()->IsDown(ICKEY_A));icInput::Instance()->Update();
    event.xkey.type=KeyRelease;XSendEvent(main.GetDisplay(),preview.GetRef(),False,KeyReleaseMask,&event);XSync(main.GetDisplay(),False);icWindow::PumpEvents();CHECK(icInput::Instance()->IsPressed(ICKEY_A));
    event={};event.xclient.type=ClientMessage;event.xclient.window=preview.GetRef();event.xclient.message_type=XInternAtom(main.GetDisplay(),"WM_PROTOCOLS",False);event.xclient.format=32;event.xclient.data.l[0]=XInternAtom(main.GetDisplay(),"WM_DELETE_WINDOW",False);
    XSendEvent(main.GetDisplay(),preview.GetRef(),False,NoEventMask,&event);XSync(main.GetDisplay(),False);icWindow::PumpEvents();CHECK(preview.ShouldClose());CHECK(!main.ShouldClose());
    preview.Cleanup();preview.Cleanup();CHECK(draw(main,0,1,1));main.Cleanup();main.Cleanup();
    std::puts("X11 platform checks passed: independent windows/contexts, transient owner, resize, key events, close and cleanup");return 0;
}
