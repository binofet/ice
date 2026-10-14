#ifndef IC_WINDOW_X11_H
#define IC_WINDOW_X11_H
#include "icWindow.h"
#ifdef X11
#include <X11/Xlib.h>
#include <GL/glx.h>
class icWindowX11 : public icWindow {
public:
    icWindowX11(const icWindowDef& def,icWindow* parent=nullptr);
    ~icWindowX11() override;
    ICRESULT Cleanup() override;
    ICRESULT Show() override;
    ICRESULT Hide() override;
    ICRESULT UpdateParams(const icWindowDef& def) override;
    ICRESULT GetWindowPos(icVector2Int* position) override;
    void GetClientSize(int* width,int* height) const override;
    void* CreateGLContext(void* shared=nullptr);
    void DestroyGLContext();
    void MakeCurrent();
    void SwapBuffers();
    Display* GetDisplay() const {return m_display;}
    ::Window GetRef() const {return m_window;}
    static void PumpEvents();
    static void SetCursorCaptured(bool capture);
    static void WarpCursor(short x,short y);
    virtual bool HandleNativeEvent(XEvent& event){return false;}
private:
    Display* m_display=nullptr;
    ::Window m_window=0;
    XVisualInfo* m_visual=nullptr;
    Colormap m_colormap=0;
    Cursor m_cursor=0;
    GLXContext m_context=nullptr;
    Atom m_delete=0;
    bool m_destroyed=false;
};
#endif
#endif
