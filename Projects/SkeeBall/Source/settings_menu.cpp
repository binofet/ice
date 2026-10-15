#include "settings_menu.h"
#include "ice/input/input.h"
#include "skeeball.h"
#include <algorithm>
#include <cstdio>
extern SkeeBall        &gApp;
std::vector<ArcadeRect> SettingsMenu::rows() const {
	int w, h;
	gApp.ui.size(w, h);
	float width = std::min(560.f, w * .85f), x = (w - width) * .5f, top = h * .57f;
	return {{x, top, width, 64}, {x, top - 90, width, 64}, {x, top - 196, width, 48}};
}
void SettingsMenu::on_push() {
	render_top_only_ = true;
	input.skip       = true;
	ice::input::input::instance()->show_curs();
}
ice::result SettingsMenu::update(float, bool *) {
	auto keys = ice::input::input::instance();
	if (input.skip) {
		input.skip = false;
		return IC_OK;
	}
	auto rows     = this->rows();
	bool activate = gApp.ui.navigate(input, rows);
	if (keys->is_pressed(ice::input::key_escape) || (activate && input.selected == 2)) {
		gApp.get_state_machine().pop_state();
		return IC_OK;
	}
	auto  settings = gApp.get_settings();
	float x, y;
	gApp.ui.pointer(x, y);
	for (int i = 0; i < 2; i++) {
		float &value = i == 0 ? settings.fSfx_vol : settings.fMusic_vol;
		float  begin = rows[i].x + rows[i].w * .45f, end = rows[i].x + rows[i].w - 24;
		if (keys->is_down(ice::input::mouse_l) && rows[i].contains(x, y)) {
			input.selected = i;
			value          = ice::math::clamp((x - begin) / (end - begin), 0.f, 1.f);
		}
		if (input.selected == i) {
			if (keys->is_pressed(ice::input::key_left))
				value = std::max(0.f, value - .05f);
			if (keys->is_pressed(ice::input::key_right))
				value = std::min(1.f, value + .05f);
		}
	}
	gApp.update_settings(settings);
	return IC_OK;
}
ice::result SettingsMenu::render() {
	int w, h;
	gApp.ui.size(w, h);
	auto rows     = this->rows();
	auto settings = gApp.get_settings();
	gApp.ui.begin();
	gApp.ui.heading(rows[0].x, h * .79f, "SETTINGS");
	gApp.ui.text(rows[0].x, h * .71f, "Make yourself comfortable.", true);
	const char *labels[] = {"SOUND FX", "MUSIC"};
	float       values[] = {settings.fSfx_vol, settings.fMusic_vol};
	for (int i = 0; i < 2; i++) {
		gApp.ui.button(rows[i], labels[i], input.selected == i);
		float begin = rows[i].x + rows[i].w * .45f, width = rows[i].w * .55f - 24;
		gApp.ui.rect({begin, rows[i].y + 20, width, 6}, ice::math::color(50, 67, 72));
		gApp.ui.rect({begin, rows[i].y + 20, width * values[i], 6}, ice::math::color(95, 196, 181));
		gApp.ui.rect({begin + width * values[i] - 3, rows[i].y + 14, 6, 18},
		             ice::math::color(241, 195, 119));
		char number[20];
		std::snprintf(number, sizeof(number), "%d%%", (int)(values[i] * 100 + .5f));
		gApp.ui.text(begin, rows[i].y + 40, number, true);
	}
	gApp.ui.button(rows[2], "BACK", input.selected == 2);
	gApp.ui.text(rows[0].x, 24, "Drag to adjust, or use arrows. Esc: back.", true);
	gApp.ui.end();
	return IC_OK;
}
