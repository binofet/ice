#pragma once
#include "arcade_ui.h"
#include "ice/util/state_machine.h"
class MainMenu : public ice::util::state {
public:
	ice::result init(ice::assets::content_loader *) { return IC_OK; }
	ice::result update(float, bool *);
	ice::result render();
	void        on_push();
	void        on_pop() {}
	bool        SkipUpdate = false;

private:
	ArcadeMenuInput         input;
	std::vector<ArcadeRect> buttons() const;
};
