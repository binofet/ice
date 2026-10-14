#include "Core/Application/Window/icWindowX11.h"
#include "Core/IO/icInput.h"
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <map>
static Display* display=nullptr;
static std::map<::Window,icWindowX11*> windows;
static icWindowX11* focused=nullptr;
static bool captureRequested=false;
static icWindowX11* captured=nullptr;
static int iceKey(KeySym key){
    if(key>=XK_a&&key<=XK_z)return ICKEY_A+key-XK_a;
    if(key>=XK_A&&key<=XK_Z)return ICKEY_A+key-XK_A;
    if(key>=XK_0&&key<=XK_9)return ICKEY_0+key-XK_0;
    if(key>=XK_F1&&key<=XK_F12)return ICKEY_F1+key-XK_F1;
    switch(key){
    case XK_Escape:return ICKEY_ESCAPE;case XK_Return:case XK_KP_Enter:return ICKEY_RETURN;
    case XK_space:return ICKEY_SPACE;case XK_Tab:return ICKEY_TAB;case XK_BackSpace:return ICKEY_BACK;
    case XK_Left:return ICKEY_LEFT;case XK_Right:return ICKEY_RIGHT;case XK_Up:return ICKEY_UP;case XK_Down:return ICKEY_DOWN;
    case XK_Shift_L:case XK_Shift_R:return ICKEY_SHIFT;case XK_Control_L:case XK_Control_R:return ICKEY_CONTROL;
    case XK_Alt_L:case XK_Alt_R:return ICKEY_MENU;case XK_Home:return ICKEY_HOME;case XK_End:return ICKEY_END;
    case XK_Page_Up:return ICKEY_PG_UP;case XK_Page_Down:return ICKEY_PG_DN;case XK_Delete:return ICKEY_DELETE;
    default:return -1;
    }
}
icWindowX11::icWindowX11(const icWindowDef& def,icWindow* parent):icWindow(def,parent){
    if(def.i16Width<=0||def.i16Height<=0)return;
    if(!display)display=XOpenDisplay(nullptr);if(!display){icWarning("Cannot open X display");return;}
    m_display=display;int screen=DefaultScreen(display);
    int attributes[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_DEPTH_SIZE,24,None};
    m_visual=glXChooseVisual(display,screen,attributes);if(!m_visual)return;
    m_colormap=XCreateColormap(display,RootWindow(display,screen),m_visual->visual,AllocNone);
    XSetWindowAttributes values={};values.colormap=m_colormap;
    values.event_mask=StructureNotifyMask|ExposureMask|KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask;
    m_window=XCreateWindow(display,RootWindow(display,screen),def.i16PosX,def.i16PosY,def.i16Width,def.i16Height,0,m_visual->depth,InputOutput,m_visual->visual,CWColormap|CWEventMask,&values);
    if(!m_window)return;
    windows[m_window]=this;m_delete=XInternAtom(display,"WM_DELETE_WINDOW",False);XSetWMProtocols(display,m_window,&m_delete,1);
    XClassHint hint={const_cast<char*>("ice"),const_cast<char*>("ICE")};XSetClassHint(display,m_window,&hint);
    if(auto owner=dynamic_cast<icWindowX11*>(parent))XSetTransientForHint(display,m_window,owner->GetRef());
    char blank=0;Pixmap pixmap=XCreateBitmapFromData(display,m_window,&blank,1,1);XColor color={};
    m_cursor=XCreatePixmapCursor(display,pixmap,pixmap,&color,&color,0,0);XFreePixmap(display,pixmap);
    UpdateParams(def);
}
icWindowX11::~icWindowX11(){Cleanup();}
ICRESULT icWindowX11::Cleanup(){
    if(!m_display)return IC_OK;
    if(focused==this)focused=nullptr;
    if(captured==this){XUngrabPointer(m_display,CurrentTime);captured=nullptr;}
    DestroyGLContext();windows.erase(m_window);
    if(m_window&&!m_destroyed)XDestroyWindow(m_display,m_window);
    if(m_cursor)XFreeCursor(m_display,m_cursor);if(m_colormap)XFreeColormap(m_display,m_colormap);if(m_visual)XFree(m_visual);
    m_window=0;m_cursor=0;m_colormap=0;m_visual=nullptr;m_display=nullptr;
    if(windows.empty()){XCloseDisplay(display);display=nullptr;}return IC_OK;
}
ICRESULT icWindowX11::Show(){if(!m_window)return IC_FAIL_CREATE_WIND;XMapRaised(m_display,m_window);XFlush(m_display);return IC_OK;}
ICRESULT icWindowX11::Hide(){if(!m_window)return IC_FAIL_CREATE_WIND;XUnmapWindow(m_display,m_window);XFlush(m_display);return IC_OK;}
ICRESULT icWindowX11::UpdateParams(const icWindowDef& def){
    if(!m_window||def.i16Width<=0||def.i16Height<=0)return IC_FAIL_GEN;
    XStoreName(m_display,m_window,def.pWindowName?def.pWindowName:"ICE");
    Atom state=XInternAtom(m_display,"_NET_WM_STATE",False),fullscreen=XInternAtom(m_display,"_NET_WM_STATE_FULLSCREEN",False);
    XEvent event={};event.xclient.type=ClientMessage;event.xclient.window=m_window;event.xclient.message_type=state;event.xclient.format=32;
    event.xclient.data.l[0]=def.bFullScreen?1:0;event.xclient.data.l[1]=fullscreen;event.xclient.data.l[3]=1;
    XSendEvent(m_display,DefaultRootWindow(m_display),False,SubstructureRedirectMask|SubstructureNotifyMask,&event);
    int screen=DefaultScreen(m_display);
    XMoveResizeWindow(m_display,m_window,def.bFullScreen?0:def.i16PosX,def.bFullScreen?0:def.i16PosY,
                     def.bFullScreen?DisplayWidth(m_display,screen):def.i16Width,def.bFullScreen?DisplayHeight(m_display,screen):def.i16Height);
    m_WindowInfo=def;XSync(m_display,False);return IC_OK;
}
ICRESULT icWindowX11::GetWindowPos(icVector2Int* position){
    if(!position||!m_window)return IC_FAIL_NPE;::Window child;
    XTranslateCoordinates(m_display,m_window,DefaultRootWindow(m_display),0,0,&position->x,&position->y,&child);return IC_OK;
}
void icWindowX11::GetClientSize(int* width,int* height)const{
    XWindowAttributes attributes={};if(m_window)XGetWindowAttributes(m_display,m_window,&attributes);
    if(width)*width=attributes.width;if(height)*height=attributes.height;
}
void* icWindowX11::CreateGLContext(void* shared){
    if(m_context)return m_context;if(!m_visual)return nullptr;
    m_context=glXCreateContext(m_display,m_visual,(GLXContext)shared,True);MakeCurrent();
    using SwapInterval=void(*)(Display*,GLXDrawable,int);
    auto swap=(SwapInterval)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");
    if(swap&&strstr(glXQueryExtensionsString(m_display,DefaultScreen(m_display)),"GLX_EXT_swap_control"))swap(m_display,m_window,1);
    return m_context;
}
void icWindowX11::DestroyGLContext(){if(!m_context)return;if(glXGetCurrentContext()==m_context)glXMakeCurrent(m_display,None,nullptr);glXDestroyContext(m_display,m_context);m_context=nullptr;}
void icWindowX11::MakeCurrent(){if(m_context)glXMakeCurrent(m_display,m_window,m_context);}
void icWindowX11::SwapBuffers(){if(m_context)glXSwapBuffers(m_display,m_window);}
void icWindowX11::SetCursorCaptured(bool capture){
    captureRequested=capture;
    if(captured){XUngrabPointer(display,CurrentTime);XUndefineCursor(display,captured->m_window);captured=nullptr;}
    if(capture&&focused){
        int result=XGrabPointer(display,focused->m_window,True,PointerMotionMask|ButtonPressMask|ButtonReleaseMask,GrabModeAsync,GrabModeAsync,focused->m_window,focused->m_cursor,CurrentTime);
        if(result==GrabSuccess){captured=focused;int width,height;focused->GetClientSize(&width,&height);WarpCursor(width/2,height/2);}
    }
}
void icWindowX11::WarpCursor(short x,short y){if(focused){XWarpPointer(display,None,focused->m_window,0,0,0,0,x,y);XFlush(display);}icInput::Instance()->ResetMouseTracking();}
void icWindowX11::PumpEvents(){
    if(!display)return;
    while(XPending(display)){
        XEvent event;XNextEvent(display,&event);auto found=windows.find(event.xany.window);if(found==windows.end())continue;
        auto window=found->second;if(window->HandleNativeEvent(event))continue;
        auto input=icInput::Instance();int width,height;window->GetClientSize(&width,&height);
        switch(event.type){
        case ClientMessage:if((Atom)event.xclient.data.l[0]==window->m_delete)window->RequestClose();break;
        case DestroyNotify:window->m_destroyed=true;window->RequestClose();break;
        case FocusIn:icPlatformEvents::Post(icPlatformEventType::FocusGained,window->GetID());focused=window;input->ResetMouseTracking();SetCursorCaptured(captureRequested);break;
        case FocusOut:icPlatformEvents::Post(icPlatformEventType::FocusLost,window->GetID());if(focused==window)focused=nullptr;SetCursorCaptured(captureRequested);input->CancelHeldInput();break;
        case KeyPress:case KeyRelease:{
            // X autorepeat emits release/press pairs; do not turn them into release edges.
            if(event.type==KeyRelease&&XPending(display)){XEvent next;XPeekEvent(display,&next);if(next.type==KeyPress&&next.xkey.keycode==event.xkey.keycode&&next.xkey.time==event.xkey.time)break;}
            int key=iceKey(XLookupKeysym(&event.xkey,0));if(key>=0)input->SetDown(key,event.type==KeyPress);break;
        }
        case ButtonPress:case ButtonRelease:{
            unsigned button=event.xbutton.button;
            if(button==4||button==5){if(event.type==ButtonPress)input->AddScroll(button==4?1:-1);}
            else input->SetDown(button==1?ICMOUSE_L:button==3?ICMOUSE_R:ICMOUSE_M,event.type==ButtonPress);
            if(!captured)input->UpdateMousePosition(event.xbutton.x,event.xbutton.y,height);break;
        }
        case MotionNotify:
            if(captured==window){int dx=event.xmotion.x-width/2,dy=event.xmotion.y-height/2;
                if(dx||dy){input->AddMouseDelta(dx,dy,height);XWarpPointer(display,None,window->m_window,0,0,0,0,width/2,height/2);XFlush(display);}
            }else input->UpdateMousePosition(event.xmotion.x,event.xmotion.y,height);
            break;
        default:break;
        }
    }
}
