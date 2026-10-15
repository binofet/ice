#include "main_menu.h"
#include "ice/input/input.h"
#include "skeeball.h"
#include <algorithm>
extern SkeeBall        &gApp;
std::vector<ArcadeRect> MainMenu::buttons() const {
	int w, h;
	gApp.ui.size(w, h);
	float                   step = h < 600 ? 48 : 58, height = step - 10;
	std::vector<ArcadeRect> boxes;
	for (int i = 0; i < 4; i++)
		boxes.push_back(
		    {w * .075f, h * .48f - i * step - height, std::min(360.f, w * .8f), height});
	return boxes;
}
ice::result MainMenu::update(float, bool *) {
	if (SkipUpdate) {
		SkipUpdate = false;
		return IC_OK;
	}
	if (ice::input::input::instance()->is_pressed(ice::input::key_escape)) {
		gApp.quit();
		return IC_OK;
	}
	if (gApp.ui.navigate(input, buttons())) {
		if (input.selected == 0)
			gApp.get_state_machine().push_state(&gApp.game_state_);
		if (input.selected == 1) {
			gApp.toggle_browser();
			gApp.get_state_machine().push_state(&gApp.game_state_);
		}
		if (input.selected == 2)
			gApp.get_state_machine().push_state(&gApp.settings_menu_);
		if (input.selected == 3)
			gApp.quit();
	}
	return IC_OK;
}
ice::result MainMenu::render() {
	int w, h;
	gApp.ui.size(w, h);
	float x = w * .075f;
	gApp.ui.begin();
	gApp.ui.heading(x, h * .77f, "SKEEBALL");
	gApp.ui.text(x, h * .68f, "THE ICE ARCADE");
	gApp.ui.text(x, h * .59f, "Good company. One more roll.");
	auto        boxes    = buttons();
	const char *labels[] = {"PLAY SOLO", "PHONE PARTY", "SETTINGS", "QUIT"};
	for (int i = 0; i < 4; i++)
		gApp.ui.button(boxes[i], labels[i], input.selected == i);
	gApp.ui.text(x, 24, "Phones welcome. No app needed.", true);
	gApp.ui.end();
	return IC_OK;
}
void MainMenu::on_push() {
	render_top_only_ = true;
	input.skip       = true;
	ice::input::input::instance()->show_curs();
}
