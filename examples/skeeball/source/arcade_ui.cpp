#include "arcade_ui.h"
#include "ice/input/input.h"
#include "skeeball.h"
#include <algorithm>
extern SkeeBall &gApp;
ice::result      ArcadeUI::init(ice::assets::content_loader *loader) {
    content       = loader;
    ice::result r = content->load("resource/textures/title.tga", &background);
    r |= title.initialize("Arial", 48, content->get_device());
    r |= body.initialize("Arial", 24, content->get_device());
    r |= small.initialize("Arial", 18, content->get_device());
    return r;
}
void ArcadeUI::size(int &w, int &h) const { gApp.get_main_window()->get_framebuffer_size(&w, &h); }
void ArcadeUI::pointer(float &x, float &y) const {
	short px, py;
	ice::input::input::instance()->get_pos(&px, &py);
	int w, h, cw, ch;
	size(w, h);
	gApp.get_main_window()->get_client_size(&cw, &ch);
	x = cw > 0 ? float(px) * w / cw : 0;
	y = ch > 0 ? float(py) * h / ch : 0;
}
void ArcadeUI::begin(bool drawBackground) {
	auto            dev = content->get_device();
	ice::math::mat4 identity;
	(identity) = ice::math::mat4(1);
	dev->set_projection(dev->get_ortho());
	dev->set_view_matrix(identity);
	dev->set_world_transform(identity);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_SCISSOR_TEST);
	if (!drawBackground)
		return;
	int w, h;
	size(w, h);
	if (w <= 0 || h <= 0)
		return;
	// Cover the live surface without stretching the original 16:9 artwork.
	float u0 = 0, v0 = 0, u1 = 1, v1 = 1, aspect = float(w) / h, artAspect = 16.f / 9;
	if (aspect > artAspect) {
		float visible = artAspect / aspect;
		v0            = (1 - visible) * .5f;
		v1            = 1 - v0;
	} else {
		float visible = aspect / artAspect;
		u0            = (1 - visible) * .5f;
		u1            = 1 - u0;
	}
	dev->set_texture(0, background);
	glDisable(GL_BLEND);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1, 1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(u0, v0);
	glVertex2f(0, 0);
	glTexCoord2f(u1, v0);
	glVertex2f(w, 0);
	glTexCoord2f(u1, v1);
	glVertex2f(w, h);
	glTexCoord2f(u0, v1);
	glVertex2f(0, h);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	glColor4f(.015f, .03f, .045f, .94f);
	glVertex2f(0, 0);
	glVertex2f(0, h);
	glColor4f(.015f, .03f, .045f, .1f);
	glVertex2f(w, h);
	glVertex2f(w, 0);
	glEnd();
}
void ArcadeUI::end() { glPopAttrib(); }
void ArcadeUI::rect(const ArcadeRect &r, const ice::math::color &color) {
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(color.red_float(), color.green_float(), color.blue_float(), color.alpha_float());
	glBegin(GL_QUADS);
	glVertex2f(r.x, r.y);
	glVertex2f(r.x + r.w, r.y);
	glVertex2f(r.x + r.w, r.y + r.h);
	glVertex2f(r.x, r.y + r.h);
	glEnd();
}
void ArcadeUI::text(float x, float y, const char *text, bool compact) {
	(compact ? small : body).print(ice::math::ivec2(x, y), ice::math::color(235, 230, 213), text);
}
void ArcadeUI::heading(float x, float y, const char *text) {
	title.print(ice::math::ivec2(x, y), ice::math::color(244, 224, 187), text);
}
void ArcadeUI::button(const ArcadeRect &r, const char *text, bool selected) {
	rect(r, selected ? ice::math::color(24, 92, 96, 242) : ice::math::color(7, 25, 32, 235));
	if (selected)
		rect({r.x, r.y, 4, r.h}, ice::math::color(242, 171, 79));
	this->text(r.x + 22, r.y + (r.h - 24) * .5f + 2, text);
}
bool ArcadeUI::navigate(ArcadeMenuInput &state, const std::vector<ArcadeRect> &boxes) {
	if (state.skip) {
		state.skip = false;
		return false;
	}
	auto input = ice::input::input::instance();
	int  count = (int)boxes.size();
	if (!count)
		return false;
	if (input->is_pressed(ice::input::key_down))
		state.selected = (state.selected + 1) % count;
	if (input->is_pressed(ice::input::key_up))
		state.selected = (state.selected + count - 1) % count;
	float x, y;
	pointer(x, y);
	int hover = -1;
	for (int i = 0; i < count; i++)
		if (boxes[i].contains(x, y))
			hover = i;
	if (hover >= 0 &&
	    (x != state.lastX || y != state.lastY || input->is_pressed(ice::input::mouse_l)))
		state.selected = hover;
	state.lastX = x;
	state.lastY = y;
	return input->is_pressed(ice::input::key_return) ||
	       (hover >= 0 && input->is_pressed(ice::input::mouse_l));
}
