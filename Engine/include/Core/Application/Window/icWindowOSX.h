#ifndef IC_WINDOW_OSX_H
#define IC_WINDOW_OSX_H
#ifdef __APPLE__
#include "Core/Application/Window/icWindow.h"
struct icOSXWindowState;
// AppKit lives in the Objective-C++ implementation; public ICE headers stay C++.
class icWindowOSX : public icWindow {
public:
    icWindowOSX(const icWindowDef& def,icWindow* parent=nullptr);
    ~icWindowOSX() override;
    ICRESULT Cleanup() override;
    ICRESULT Show() override;
    ICRESULT Hide() override;
    ICRESULT UpdateParams(const icWindowDef& def) override;
    ICRESULT GetWindowPos(icVector2Int* position) override;
    void GetClientSize(int* width,int* height) const override;
    void GetFramebufferSize(int* width,int* height) const override;
    // Borrowed NSWindow/NSView pointers. No ownership transfer.
    void* GetRef() const;
    void* GetView() const;
    void* CreateGLContext(void* sharedContext=nullptr);
    void DestroyGLContext();
    void MakeCurrent();
    void SwapBuffers();
    // Return true to consume a borrowed NSEvent before AppKit handles it.
    virtual bool HandleNativeEvent(void* event){return false;}
    static void PumpEvents();
    static void SetCursorCaptured(bool captured);
    static void WarpCursor(short x,short y);
    static void FocusChanged();
private:
    icOSXWindowState* m_native=nullptr;
};
#endif
#endif
