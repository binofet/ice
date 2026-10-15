#include "SampleHost.h"
#include "Core/GXDevice/icGXDeviceGL.h"
#include "Render/Font/icFont.h"
#include <memory>

#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"GL render check failed line %d: %s\n",__LINE__,#x);return 1;} } while(0)

static void triangle(float z, bool red) {
    glColor3f(red?1:0,0,red?0:1);
    glBegin(GL_TRIANGLES);
    glVertex3f(-1,-1,z);glVertex3f(1,-1,z);glVertex3f(0,1,z);
    glEnd();
}
static bool occlusion(bool nearFirst,bool expectRed) {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(nearFirst){triangle(-.5f,true);triangle(.5f,false);}
    else {triangle(.5f,false);triangle(-.5f,true);}
    unsigned char pixel[3]={};GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(viewport[2]/2,viewport[3]/2,1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel);
    return glGetError()==GL_NO_ERROR && pixel[expectRed?0:2]>240 && pixel[expectRed?2:0]<10;
}
int main() {
    sample::initialize();icWindowDef def={0,160,120,0,0,"ICE depth regression",false};
    std::unique_ptr<icWindow> window(icWindow::GetWindow(def));CHECK(window);
    // Declared after window so GL resources are destroyed before the context/window.
    icGXDeviceGL device;CHECK(!ICEFAIL(device.Init(window.get())));
    CHECK(glIsEnabled(GL_DEPTH_TEST));GLint bits=0;glGetIntegerv(GL_DEPTH_BITS,&bits);CHECK(bits>=16);
    device.FrameBegin();
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    // Near geometry must win in either submission order without any UI draw first.
    CHECK(occlusion(true,true));CHECK(occlusion(false,true));
    CHECK(!ICEFAIL(device.EnableDepthTest(false)));CHECK(!glIsEnabled(GL_DEPTH_TEST));
    CHECK(occlusion(true,false)); // verifies the fixture actually detects missing depth
    CHECK(!ICEFAIL(device.EnableDepthTest(true)));CHECK(occlusion(true,true));
    // A font overlay must not silently disable depth for the next 3D draw.
    icFontI* font=nullptr;icFontParams params={"Arial",20};CHECK(!ICEFAIL(device.CreateFont(&font,&params)));
    CHECK(!ICEFAIL(font->Print(icVector2Int(4,20),icColor::WHITE,"Overlay")));
    CHECK(glIsEnabled(GL_DEPTH_TEST));CHECK(!ICEFAIL(device.ReleaseFont(font)));
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);
    CHECK(occlusion(true,true));
    std::puts("GL depth passed: default state, both draw orders, disabled control and overlay restoration");
}
