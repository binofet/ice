#pragma once
#include "Content/icContentLoader.h"
#include "Render/Font/icFontGL.h"
#include <vector>
struct ArcadeRect {float x,y,w,h;bool Contains(float px,float py)const{return px>=x&&px<=x+w&&py>=y&&py<=y+h;}};
struct ArcadeMenuInput {int selected=0;float lastX=-1,lastY=-1;bool skip=true;};
class ArcadeUI {
public:
    ICRESULT Init(icContentLoader*);
    void Size(int& w,int& h) const;
    void Pointer(float& x,float& y) const;
    void Begin(bool background=true);
    void End();
    void Rect(const ArcadeRect&,const icColor&);
    void Text(float x,float y,const char*,bool small=false);
    void Heading(float x,float y,const char*);
    void Button(const ArcadeRect&,const char*,bool selected);
    bool Navigate(ArcadeMenuInput&,const std::vector<ArcadeRect>&);
    icFontGL title,body,small;
private:
    icContentLoader* content=nullptr;
    icTexture* background=nullptr;
};
