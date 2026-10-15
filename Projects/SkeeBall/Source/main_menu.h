#pragma once
#include "Util/icStateMachine.h"
#include "Content/icContentLoader.h"
#include "Render/Font/icFontGL.h"
class icTexture;
class MainMenu : public icState {
public:
    ICRESULT Init(icContentLoader*);
    ICRESULT Update(float,bool*);
    ICRESULT Render();
    void OnPush();
    void OnPop();
    bool SkipUpdate=false;
private:
    icContentLoader* content=nullptr;
    icTexture* background=nullptr;
    icFontGL title,body;
    int selected=0;
    short lastX=-1,lastY=-1;
};
