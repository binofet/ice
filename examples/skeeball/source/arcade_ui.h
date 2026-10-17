#pragma once
#include "ice/assets/content_loader.h"
#include "ice/render/gl/font_gl.h"
#include <vector>
struct ArcadeRect {
	float x, y, w, h;
	bool  contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
	}
};
struct ArcadeMenuInput {
	int   selected = 0;
	float lastX = -1, lastY = -1;
	bool  skip = true;
};
class ArcadeUI {
public:
	ice::result              init(ice::assets::content_loader *);
	void                     size(int &w, int &h) const;
	void                     pointer(float &x, float &y) const;
	void                     begin(bool background = true);
	void                     end();
	void                     rect(const ArcadeRect &, const ice::math::color &);
	void                     text(float x, float y, const char *, bool small = false);
	void                     heading(float x, float y, const char *);
	void                     button(const ArcadeRect &, const char *, bool selected);
	bool                     navigate(ArcadeMenuInput &, const std::vector<ArcadeRect> &);
	ice::render::gl::font_gl title, body, small;

private:
	ice::assets::content_loader *content    = nullptr;
	ice::render::texture        *background = nullptr;
};
