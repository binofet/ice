
#include "ice/render/camera.h"
namespace ice::render {

// #include "ice/render/gl/api.h"
// #include "GL/glu.h"

/*! Converts a world space position into screen space
 *
 *  @param              world           The position to convert
 *  @param              width           Width of screen
 *  @param              height          Height of screen
 *  @returns            icVector2       Screen position
 **/
ice::math::vec2 camera::world_to_screen(const ice::math::vec3 &world, int width, int height) {
	ice::math::vec2 screen_pos;
	ice::math::vec4 v(0.0f, 0.0f, 0.0f, 1.0f);
	ice::math::vec4 world4(world.x, world.y, world.z, 1.0f);

	v = view_mat_ * projection_ * world4;

	screen_pos.x = (v.x / v.w + 1.0f) / 2.0f * (float)width;
	screen_pos.y *= (-v.y / v.w + 1.0f) / 2.0f * (float)height;

	////Move source point into screen space
	// Vector4 source = Vector4.Transform(position, SpacewarGame.Camera.View *
	// SpacewarGame.Camera.Projection);
	////and into pixels
	// Vector2 source2D = new Vector2((int)((source.X / source.W + 1f) / 2f * 1280),
	// (int)((-source.Y / source.W + 1f) / 2f * 720));

#ifdef IAMRETARDED
	int wind[4];

	GLdouble model[16];
	GLdouble proj[16];

	GLdouble x, y, z;

	ice::math::mat4 temp_mat = m_m44ViewMat.Inverse();
	for (int i = 0; i < 16; ++i) {
		model[i] = (GLdouble)temp_mat[i];
		proj[i]  = (GLdouble)m_m44Projection[i];
	}

	glGetIntegerv(GL_VIEWPORT, wind);
	gluProject(world.x, world.y, world.z, model, proj, wind, &x, &y, &z);
#endif

	return screen_pos;
}

/*! Converts a screen space position into a world direction
 *
 *  @param              pos           The position to convert
 *  @param              size          The screen size
 *  @returns            icVector3       World direction
 **/
ice::math::vec3 camera::screen_to_world_dir(const ice::math::vec2  &pos,
                                            const ice::math::ivec2 &size) const {
	ice::math::vec3 ray_to;
	ray_to.x = (((2.0f * pos.x) / (ice::math::real)size.x) - 1) / (projection_)[0][0];
	ray_to.y = (((2.0f * pos.y) / (ice::math::real)size.y) - 1) / (projection_)[1][1];
	ray_to.z = 1.0f;

	ray_to =
	    ice::math::transform_direction(view_mat_, ice::math::vec3(ray_to.x, ray_to.y, ray_to.z));

	return ray_to;
}

} // namespace ice::render
