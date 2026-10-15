#pragma once

#include "ice/render/graphics_device.h"
namespace ice::render::gl {

#ifdef ICGL
} // namespace ice::render::gl
#include "ice/render/gl/api.h"

#include "ice/render/vert_buffer.h"
namespace ice::render::gl {

/*! converts IC_PRIM to GL primitive types
 **/
FORCE_INLINE GLenum prim_to_gl(ice::render::prim prim) {
	switch (prim) {
	case ice::render::ic_prim_plist:
		return GL_POINTS;
	case ice::render::prim_llist:
		return GL_LINES;
	case ice::render::prim_lstrip:
		return GL_LINE_STRIP;
	case ice::render::prim_tlist:
		return GL_TRIANGLES;
	case ice::render::prim_tstrip:
		return GL_TRIANGLE_STRIP;
	case ice::render::prim_tfan:
		return GL_TRIANGLE_FAN;
	default:
		warningf("INVAID IC_PRIM - %i", prim);
	}
	return 0;
};

/*! Implements the icGXDevice using the openGL API
 **/
class graphics_device : public ice::render::graphics_device {
public:
	graphics_device(void);
	~graphics_device(void);

	ice::result cleanup(void);
	ice::result init(class ice::platform::window *const window);

	virtual ice::result update(void);

	virtual void frame_begin(void);
	virtual void frame_end(bool swap = true);

	virtual ice::result draw_vert_buf(class ice::render::vert_buffer *vb, ice::uint offset = 0,
	                                  ice::uint count = 0);
	virtual ice::result draw_indexed_vert_buf(class ice::render::vert_buffer *vb,
	                                          class ice::render::index_buf   *ib);

	virtual ice::result create_vb(class ice::render::vert_buffer **out_vertex_buffer,
	                              struct ice::render::vert_def    *description);
	virtual ice::result release_vb(class ice::render::vert_buffer *vb);

	virtual ice::result create_ib(class ice::render::index_buf **out_index_buffer,
	                              struct ice::render::index_def *description);
	virtual ice::result release_ib(class ice::render::index_buf *ib);

	virtual ice::result create_tex(class ice::render::texture **out_texture,
	                               struct ice::render::tex_def *description);
	virtual ice::result create_tex_from_file(class ice::render::texture **out_texture,
	                                         const char                  *file);
	virtual ice::result release_tex(class ice::render::texture *tex);

	/// Create a device-owned font in the current context; release the output before destroying the
	/// device.
	virtual ice::result create_font(class ice::render::font        **out_font,
	                                struct ice::render::font_params *parameters);
	/// Release a font created by this device before destroying its graphics context.
	virtual ice::result release_font(class ice::render::font *font);

	virtual ice::result enable_alpha_blend(bool alpha_blend);
	virtual ice::result enable_depth_test(bool depth);
	virtual ice::result enable_fog(bool enable, const ice::render::fog_params &params);

	virtual ice::result set_ambient_color(const ice::math::color &color);
	virtual ice::result set_clear_color(const ice::math::color &color);
	virtual ice::result set_cull_mode(ice::render::winding_cull_mode mode);
	virtual ice::result set_projection(const ice::math::mat4 &projection);
	virtual ice::result set_view_matrix(const ice::math::mat4 &view);
	virtual ice::result set_world_transform(const ice::math::mat4 &world);

	virtual ice::result set_texture(ice::uint index, ice::render::texture *tex);

	/// CALLBACKS
	virtual void change_size(ice::ushort size_x, ice::ushort size_y);

#ifdef WIN32
	/// Borrow the native device context; validity ends at device cleanup.
	FORCE_INLINE HDC get_device_context(void) { return device_context_; };
	/// Borrowed HGLRC for native integration such as context resource sharing.
	/// Borrow the native GL rendering context; do not delete it through this handle.
	FORCE_INLINE HGLRC get_render_context(void) { return render_context_; };
#endif

protected:
	/// HELPER FUNCTIONS
	ice::result create_device(void);

private:
#ifdef WIN32
	HDC   device_context_;
	HGLRC render_context_;
#elif defined(__APPLE__)
	void *native_context_;
#elif defined(X11)
	void *native_context_;
#endif

	//! Strings returned from the gl driver defining capabilities
	const char *ps_gllib_;
	const char *ps_gl_ext_;
	const char *ps_gl_vendor_;
	const char *ps_gl_render_;
	const char *ps_gl_shader_ver_;

	ice::math::mat4 view_matrix_;
	bool            hardware_buffers_;
};
#endif // ICGL

} // namespace ice::render::gl
