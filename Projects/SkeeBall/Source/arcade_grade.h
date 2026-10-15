#pragma once
#include "ice/render/gl/pipeline.h"

// Compatibility-profile post pass, deliberately independent of scene materials.
// The title and lobby stay ungraded for crisp type and reliably scannable QR.
class ArcadeGrade {
public:
	void apply(int width, int height);
	void release();

private:
	ice::render::gl::program shader;
	GLuint                   texture = 0;
	int                      w = 0, h = 0;
	bool                     attempted = false;
};
