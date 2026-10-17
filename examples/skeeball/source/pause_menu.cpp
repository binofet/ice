#include "pause_menu.h"
#include "ice/input/input.h"
#include "skeeball.h"
#include <algorithm>
extern SkeeBall        &gApp;
std::vector<ArcadeRect> PauseMenu::buttons() const {
	int w, h;
	gApp.ui.size(w, h);
	float                   width = std::min(420.f, w * .85f), x = (w - width) * .5f;
	std::vector<ArcadeRect> result;
	for (int i = 0; i < 3; i++)
		result.push_back({x, h * .56f - i * 66, width, 48});
	return result;
}
void PauseMenu::on_push() {
	render_top_only_ = true;
	input.skip       = true;
	ice::input::input::instance()->show_curs();
}
ice::result PauseMenu::update(float, bool *) {
	if (SkipUpdate) {
		SkipUpdate = false;
		return IC_OK;
	}
	auto keys = ice::input::input::instance();
	if (input.skip) {
		input.skip = false;
		return IC_OK;
	}
	bool activate = gApp.ui.navigate(input, buttons());
	if (keys->is_pressed(ice::input::key_escape) || (activate && input.selected == 0)) {
		gApp.get_state_machine().pop_state();
		ice::input::input::instance()->hide_curs();
		return IC_OK;
	}
	if (activate && input.selected == 1)
		gApp.get_state_machine().push_state(&gApp.settings_menu_);
	if (activate && input.selected == 2) {
		while (gApp.get_state_machine().peek_state() &&
		       gApp.get_state_machine().peek_state() != &gApp.main_menu_)
			gApp.get_state_machine().pop_state();
		gApp.main_menu_.on_push();
	}
	return IC_OK;
}
ice::result PauseMenu::render() {
	int w, h;
	gApp.ui.size(w, h);
	auto boxes = buttons();
	gApp.ui.begin();
	gApp.ui.heading(boxes[0].x, h * .76f, "TAKE A BREATHER");
	const char *labels[] = {"RESUME", "SETTINGS", "TITLE SCREEN"};
	for (int i = 0; i < 3; i++)
		gApp.ui.button(boxes[i], labels[i], input.selected == i);
	gApp.ui.text(boxes[0].x, 24, "Esc: back to the lane.", true);
	gApp.ui.end();
	return IC_OK;
}
