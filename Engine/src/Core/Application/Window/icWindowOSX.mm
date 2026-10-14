#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>
#import <ApplicationServices/ApplicationServices.h>
#include "Core/Application/Window/icWindowOSX.h"
#include "Core/IO/icInput.h"
#include <vector>
#include <algorithm>

struct icOSXWindowState {
    NSWindow* window=nil;
    NSView* view=nil;
    NSObject* delegate=nil;
    NSOpenGLContext* context=nil;
    NSRect windowedFrame;
};
static std::vector<icWindowOSX*> windows;
static bool captureRequested=false,cursorHidden=false;
static icWindowOSX* focusedWindow(){
    for(auto window:windows)if((NSWindow*)window->GetRef()==[NSApp keyWindow])return window;
    return nullptr;
}
static void syncCursor(){
    bool capture=captureRequested&&[NSApp isActive]&&focusedWindow();
    if(capture==cursorHidden)return;
    if(capture){[NSCursor hide];CGAssociateMouseAndMouseCursorPosition(false);}
    else {CGAssociateMouseAndMouseCursorPosition(true);[NSCursor unhide];}
    cursorHidden=capture;
}
static int keyForEvent(NSEvent* event){
    NSString* text=[event charactersIgnoringModifiers];
    if(![text length])return -1;
    unichar c=[text characterAtIndex:0];
    if(c>='a'&&c<='z')return ICKEY_A+c-'a';
    if(c>='A'&&c<='Z')return ICKEY_A+c-'A';
    if(c>='0'&&c<='9')return ICKEY_0+c-'0';
    if(c>=NSF1FunctionKey&&c<=NSF12FunctionKey)return ICKEY_F1+c-NSF1FunctionKey;
    switch(c){
    case 27:return ICKEY_ESCAPE;case '\r':case NSEnterCharacter:return ICKEY_RETURN;
    case ' ':return ICKEY_SPACE;case '\t':return ICKEY_TAB;case NSDeleteCharacter:return ICKEY_BACK;
    case NSLeftArrowFunctionKey:return ICKEY_LEFT;case NSRightArrowFunctionKey:return ICKEY_RIGHT;
    case NSUpArrowFunctionKey:return ICKEY_UP;case NSDownArrowFunctionKey:return ICKEY_DOWN;
    case NSHomeFunctionKey:return ICKEY_HOME;case NSEndFunctionKey:return ICKEY_END;
    case NSPageUpFunctionKey:return ICKEY_PG_UP;case NSPageDownFunctionKey:return ICKEY_PG_DN;
    case NSDeleteFunctionKey:return ICKEY_DELETE;default:return -1;
    }
}
@interface ICEView : NSView { @public icWindowOSX* owner; }
@end
@implementation ICEView
- (BOOL)acceptsFirstResponder{return YES;}
- (BOOL)acceptsFirstMouse:(NSEvent*)event{return YES;}
- (void)keyDown:(NSEvent*)event{int key=keyForEvent(event);if(key>=0)icInput::Instance()->SetDown(key,true);}
- (void)keyUp:(NSEvent*)event{int key=keyForEvent(event);if(key>=0)icInput::Instance()->SetDown(key,false);}
- (void)flagsChanged:(NSEvent*)event{
    auto input=icInput::Instance();auto flags=[event modifierFlags];
    input->SetDown(ICKEY_SHIFT,(flags&NSEventModifierFlagShift)!=0);
    input->SetDown(ICKEY_CONTROL,(flags&NSEventModifierFlagControl)!=0);
    input->SetDown(ICKEY_MENU,(flags&NSEventModifierFlagOption)!=0);
}
- (void)move:(NSEvent*)event{
    if(!owner)return;
    NSPoint point=[self convertPoint:[event locationInWindow] fromView:nil];
    int height=(int)[self bounds].size.height;
    auto input=icInput::Instance();
    if(cursorHidden)input->AddMouseDelta([event deltaX],[event deltaY],height);
    else input->UpdateMousePosition(point.x,height-point.y,height);
}
- (void)mouseMoved:(NSEvent*)event{[self move:event];}
- (void)mouseDragged:(NSEvent*)event{[self move:event];}
- (void)rightMouseDragged:(NSEvent*)event{[self move:event];}
- (void)otherMouseDragged:(NSEvent*)event{[self move:event];}
- (void)mouseDown:(NSEvent*)event{[self move:event];icInput::Instance()->SetDown(ICMOUSE_L,true);}
- (void)mouseUp:(NSEvent*)event{[self move:event];icInput::Instance()->SetDown(ICMOUSE_L,false);}
- (void)rightMouseDown:(NSEvent*)event{icInput::Instance()->SetDown(ICMOUSE_R,true);}
- (void)rightMouseUp:(NSEvent*)event{icInput::Instance()->SetDown(ICMOUSE_R,false);}
- (void)otherMouseDown:(NSEvent*)event{icInput::Instance()->SetDown(ICMOUSE_M,true);}
- (void)otherMouseUp:(NSEvent*)event{icInput::Instance()->SetDown(ICMOUSE_M,false);}
- (void)scrollWheel:(NSEvent*)event{icInput::Instance()->AddScroll([event scrollingDeltaY]);}
@end
@interface ICEWindowDelegate : NSObject<NSWindowDelegate> { @public icWindowOSX* owner; }
@end
@implementation ICEWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender{if(owner)owner->RequestClose();return NO;}
- (void)windowDidResignKey:(NSNotification*)notification{if(owner)icPlatformEvents::Post(icPlatformEventType::FocusLost,owner->GetID());icWindowOSX::FocusChanged();}
- (void)windowDidBecomeKey:(NSNotification*)notification{if(owner)icPlatformEvents::Post(icPlatformEventType::FocusGained,owner->GetID());icWindowOSX::FocusChanged();}
@end
@interface ICEApplicationDelegate : NSObject<NSApplicationDelegate>
@end
@implementation ICEApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender{
    icPlatformEvents::Post(icPlatformEventType::ApplicationQuitRequested);
    for(auto window:windows)window->RequestClose();return NSTerminateCancel;
}
- (void)applicationDidResignActive:(NSNotification*)notification{icWindowOSX::FocusChanged();}
- (void)applicationDidBecomeActive:(NSNotification*)notification{icWindowOSX::FocusChanged();}
@end
static void initializeApplication(){
    static bool initialized=false;if(initialized)return;
    [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    static ICEApplicationDelegate* delegate=[[ICEApplicationDelegate alloc] init];[NSApp setDelegate:delegate];
    NSMenu* bar=[[NSMenu alloc] initWithTitle:@""];
    NSMenuItem* application=[[NSMenuItem alloc] initWithTitle:@"ICE" action:nil keyEquivalent:@""];
    NSMenu* menu=[[NSMenu alloc] initWithTitle:@"ICE"];
    [menu addItemWithTitle:@"Hide ICE" action:@selector(hide:) keyEquivalent:@"h"];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Quit ICE" action:@selector(terminate:) keyEquivalent:@"q"];
    [application setSubmenu:menu];[bar addItem:application];[NSApp setMainMenu:bar];
    [menu release];[application release];[bar release];
    [NSApp finishLaunching];initialized=true;
}
icWindowOSX::icWindowOSX(const icWindowDef& def,icWindow* parent):icWindow(def,parent){
    @autoreleasepool {
        if(![NSThread isMainThread]||def.i16Width<=0||def.i16Height<=0)return;
        initializeApplication();m_native=new icOSXWindowState;
        NSUInteger style=def.ui32WinFlags?def.ui32WinFlags:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable);
        NSRect rect=NSMakeRect(def.i16PosX,def.i16PosY,def.i16Width,def.i16Height);
        m_native->window=[[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:NO];
        [m_native->window setReleasedWhenClosed:NO];
        ICEView* view=[[ICEView alloc] initWithFrame:NSMakeRect(0,0,def.i16Width,def.i16Height)];view->owner=this;
        [view setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];[view setWantsBestResolutionOpenGLSurface:NO];
        m_native->view=view;[m_native->window setContentView:view];[view release];
        ICEWindowDelegate* delegate=[[ICEWindowDelegate alloc] init];delegate->owner=this;m_native->delegate=delegate;
        [m_native->window setDelegate:delegate];[m_native->window setAcceptsMouseMovedEvents:YES];
        [m_native->window makeFirstResponder:view];
        if(parent)if(auto native=dynamic_cast<icWindowOSX*>(parent))[(NSWindow*)native->GetRef() addChildWindow:m_native->window ordered:NSWindowAbove];
        windows.push_back(this);UpdateParams(def);
        if(!def.i16PosX&&!def.i16PosY)[m_native->window center];
    }
}
icWindowOSX::~icWindowOSX(){Cleanup();}
ICRESULT icWindowOSX::Cleanup(){
    if(!m_native)return IC_OK;
    @autoreleasepool {
        windows.erase(std::remove(windows.begin(),windows.end(),this),windows.end());syncCursor();
        DestroyGLContext();((ICEView*)m_native->view)->owner=nullptr;
        ((ICEWindowDelegate*)m_native->delegate)->owner=nullptr;
        [m_native->window setDelegate:nil];
        if([m_native->window parentWindow])[[m_native->window parentWindow] removeChildWindow:m_native->window];
        [m_native->window orderOut:nil];[m_native->window close];[m_native->window release];[m_native->delegate release];
        delete m_native;m_native=nullptr;
    }return IC_OK;
}
void* icWindowOSX::GetRef()const{return m_native?m_native->window:nil;}
void* icWindowOSX::GetView()const{return m_native?m_native->view:nil;}
ICRESULT icWindowOSX::Show(){if(!m_native)return IC_FAIL_CREATE_WIND;@autoreleasepool{[m_native->window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];syncCursor();}return IC_OK;}
ICRESULT icWindowOSX::Hide(){if(!m_native)return IC_FAIL_CREATE_WIND;@autoreleasepool{[m_native->window orderOut:nil];syncCursor();}return IC_OK;}
ICRESULT icWindowOSX::UpdateParams(const icWindowDef& def){
    if(!m_native||def.i16Width<=0||def.i16Height<=0)return IC_FAIL_GEN;
    @autoreleasepool {
        NSString* title=[NSString stringWithUTF8String:def.pWindowName?def.pWindowName:"ICE"];[m_native->window setTitle:title];
        if(def.bFullScreen){
            if(!m_WindowInfo.bFullScreen||!m_native->windowedFrame.size.width)m_native->windowedFrame=[m_native->window frame];
            [m_native->window setStyleMask:NSWindowStyleMaskBorderless];
            [m_native->window setFrame:[[m_native->window screen] frame] display:YES];
        }else{
            [m_native->window setStyleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable];
            if(m_WindowInfo.bFullScreen)[m_native->window setFrame:m_native->windowedFrame display:YES];
            [m_native->window setContentSize:NSMakeSize(def.i16Width,def.i16Height)];
        }
        m_WindowInfo=def;if(m_native->context)[m_native->context update];
    }return IC_OK;
}
ICRESULT icWindowOSX::GetWindowPos(icVector2Int* position){
    if(!m_native||!position)return IC_FAIL_NPE;
    NSRect frame=[m_native->window frame];position->x=(int)frame.origin.x;position->y=(int)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame])-NSMaxY(frame));return IC_OK;
}
void icWindowOSX::GetClientSize(int* width,int* height)const{
    NSSize size=m_native?[m_native->view bounds].size:NSZeroSize;if(width)*width=(int)size.width;if(height)*height=(int)size.height;
}
void icWindowOSX::GetFramebufferSize(int* width,int* height)const{GetClientSize(width,height);}
void* icWindowOSX::CreateGLContext(void* shared){
    if(!m_native)return nullptr;if(m_native->context)return m_native->context;
    @autoreleasepool {
        NSOpenGLPixelFormatAttribute attributes[]={NSOpenGLPFAOpenGLProfile,NSOpenGLProfileVersionLegacy,NSOpenGLPFADoubleBuffer,NSOpenGLPFAColorSize,24,NSOpenGLPFAAlphaSize,8,NSOpenGLPFADepthSize,24,0};
        NSOpenGLPixelFormat* format=[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
        if(!format)return nullptr;
        m_native->context=[[NSOpenGLContext alloc] initWithFormat:format shareContext:(NSOpenGLContext*)shared];[format release];
        [m_native->context setView:m_native->view];[m_native->context makeCurrentContext];
        GLint interval=1;[m_native->context setValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
        return m_native->context;
    }
}
void icWindowOSX::DestroyGLContext(){
    if(!m_native||!m_native->context)return;
    if([NSOpenGLContext currentContext]==m_native->context)[NSOpenGLContext clearCurrentContext];
    [m_native->context clearDrawable];[m_native->context release];m_native->context=nil;
}
void icWindowOSX::MakeCurrent(){if(m_native&&m_native->context){[m_native->context makeCurrentContext];[m_native->context update];}}
void icWindowOSX::SwapBuffers(){if(m_native&&m_native->context)[m_native->context flushBuffer];}
void icWindowOSX::PumpEvents(){
    if(!NSApp)return;
    @autoreleasepool {
        NSEvent* event;
        while((event=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])){
            bool handled=false;
            for(auto window:windows)if((NSWindow*)window->GetRef()==[event window]){handled=window->HandleNativeEvent(event);break;}
            if(!handled)[NSApp sendEvent:event];
        }
        [NSApp updateWindows];syncCursor();
    }
}
void icWindowOSX::FocusChanged(){
    auto input=icInput::Instance();input->CancelHeldInput();syncCursor();
}
void icWindowOSX::SetCursorCaptured(bool captured){captureRequested=captured;syncCursor();}
void icWindowOSX::WarpCursor(short x,short y){
    auto window=focusedWindow();if(!window)return;
    NSWindow* native=(NSWindow*)window->GetRef();NSView* view=(NSView*)window->GetView();
    NSPoint point=[view convertPoint:NSMakePoint(x,[view bounds].size.height-y) toView:nil];
    point=[native convertPointToScreen:point];CGFloat top=NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]);
    CGWarpMouseCursorPosition(CGPointMake(point.x,top-point.y));icInput::Instance()->ResetMouseTracking();
}
