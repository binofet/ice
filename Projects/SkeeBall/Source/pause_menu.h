#pragma once
#include "Util/icStateMachine.h"
#include "arcade_ui.h"
class PauseMenu : public icState {
public:
    ICRESULT Init(icContentLoader*){return IC_OK;}
    ICRESULT Update(float,bool*);
    ICRESULT Render();
    void OnPush();
    void OnPop(){}
    bool SkipUpdate=false;
private:
    ArcadeMenuInput input;
    std::vector<ArcadeRect> Buttons() const;
};
