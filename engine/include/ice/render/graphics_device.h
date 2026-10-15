#pragma once
namespace ice::platform {
class window;
}
namespace ice::render {} // namespace ice::render
#include "ice/globals.h"

#include "ice/math/color.h"
#include "ice/math/mat3.h"
#include "ice/math/mat4.h"

#include "ice/util/file_name.h"
#include "ice/util/time.h"
namespace ice::render {

/// Legacy front-face winding/culling policy; independent of programmable pipeline cull_mode.
enum winding_cull_mode { ic_cull_clockwise = 0, ic_cull_anticlock, ic_cull_off };

/// Legacy device and backbuffer dimensions in pixels, plus window/material initialization options.
struct device_setup {
	/// Logical/native display width in pixels.
	ice::uint screen_width;
	/// Logical/native display height in pixels.
	ice::uint screen_height;
	/// Drawable backbuffer width in pixels.
	ice::uint backbuf_width;
	/// Drawable backbuffer height in pixels.
	ice::uint backbuf_height;
	/// Request a decorated window rather than fullscreen presentation.
	bool windowed;
	/// Enable the legacy device material defaults.
	bool use_default_materials;
};

/// Legacy fog parameters: GL mode token, linear start/end distances in world units, and exponential
/// density.
struct fog_params {
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	friend bool operator==(const fog_params &lhs, const fog_params &rhs) {
		return (rhs.color == lhs.color && rhs.density == lhs.density && rhs.start == lhs.start &&
		        rhs.end == lhs.end && rhs.type == lhs.type);
	};

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	friend bool operator!=(const fog_params &lhs, const fog_params &rhs) {
		return (rhs.color != lhs.color || rhs.density != lhs.density || rhs.start != lhs.start ||
		        rhs.end != lhs.end || rhs.type != lhs.type);
	};

	/// Color value used by this legacy state record.
	ice::math::color color;
	/// Exponential fog density coefficient.
	float density;
	/// End time/distance in the units defined by the enclosing record.
	float end;
	/// Start time/distance in the units defined by the enclosing record.
	float start;
	/// Legacy OpenGL fog mode token.
	int type;
};

/// Legacy immediate-mode device contract. Native context operations and drawing are restricted to
/// the owning thread.
class graphics_device {
public:
	/// Bit mask selecting color/depth attachment clearing for the legacy device.
	enum clear_flags { clear_nothing = 0, clear_color = 1 << 0, clear_depth = 1 << 1 };

	graphics_device(void);

	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~graphics_device(void) { cleanup(); };

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual ice::result cleanup(void) { return IC_OK; };
	/// Initialize against the supplied borrowed dependencies; inspect the result before using the
	/// subsystem.
	virtual ice::result init(class ice::platform::window *const window) = 0;

	virtual ice::result update(void);

	/// Begin drawing a legacy device frame in its current native context.
	virtual void frame_begin(void) = 0;
	virtual void frame_end(bool swap = true);

	/// Draw a vertex-buffer range in the current context; zero count selects the backends
	/// remaining/full range.
	virtual ice::result draw_vert_buf(class vert_buffer *vb, ice::uint offset = 0,
	                                  ice::uint count = 0) = 0;
	/// Owned index-buffer interface. Lock/unlock CPU access before drawing or releasing the buffer.
	/// Draw indexed geometry in the current context; unsupported client-memory buffers report
	/// not_implemented.
	/// Owned index-buffer interface; locked storage is borrowed until unlock and must not outlive
	/// the buffer.
	virtual ice::result draw_indexed_vert_buf(class vert_buffer *vb, class index_buf *ib) = 0;

	/// Create a device-owned vertex buffer and return it through the output pointer; release it
	/// through this device.
	virtual ice::result create_vb(class vert_buffer **out_vertex_buffer,
	                              struct vert_def    *description) = 0;
	/// Release a vertex buffer created by this device; its native names and lock pointers become
	/// invalid.
	virtual ice::result release_vb(class vert_buffer *vb) = 0;

	/// Index count, element width, and usage metadata for one index buffer. Create a device-owned
	/// index buffer and return it through the output pointer; release it through this device.
	/// Index count, element width, primitive topology, and usage for a legacy index buffer.
	virtual ice::result create_ib(class index_buf **out_index_buffer,
	                              struct index_def *description) = 0;
	/// Release an index buffer created by this device; its native names and lock pointers become
	/// invalid.
	virtual ice::result release_ib(class index_buf *ib) = 0;

	/// Create a device-owned texture from the description; release the output through this device.
	virtual ice::result create_tex(class texture **out_texture, struct tex_def *description) = 0;
	/// Load a legacy file texture into a device-owned object; release the output through this
	/// device.
	virtual ice::result create_tex_from_file(class texture **out_texture, const char *file) = 0;
	/// Release a texture created by this device; borrowed texture/lock handles become invalid.
	virtual ice::result release_tex(class texture *tex) = 0;

	/// Font family, logical glyph size, weight/style, and optional native/device initialization
	/// data. Create a device-owned font in the current context; release the output before
	/// destroying the device. Owned font family name and logical glyph size for native font
	/// creation.
	virtual ice::result create_font(class font **out_font, struct font_params *parameters) = 0;
	/// Release a font created by this device before destroying its graphics context.
	virtual ice::result release_font(class font *font) = 0;

	/// Enable/disable conventional alpha blending on the current legacy device.
	virtual ice::result enable_alpha_blend(bool alpha_blend) = 0;
	/// Enable/disable depth comparison on the current legacy device.
	virtual ice::result enable_depth_test(bool depth) = 0;
	/// Enable/disable legacy fog using the supplied world-distance/density parameters.
	virtual ice::result enable_fog(bool enable, const fog_params &params) = 0;

	/// Set the legacy ambient-light color in the current context.
	virtual ice::result set_ambient_color(const ice::math::color &color) = 0;
	/// Set the color used when clearing the current framebuffer.
	virtual ice::result set_clear_color(const ice::math::color &color) = 0;
	/// Set legacy culling/front-face winding policy for subsequent draws.
	virtual ice::result set_cull_mode(winding_cull_mode mode) = 0;
	/// Set the legacy camera-to-clip projection matrix without changing its depth convention.
	virtual ice::result set_projection(const ice::math::mat4 &projection) = 0;
	/// Accept a camera-to-world pose and invert it to establish the legacy view transform.
	virtual ice::result set_view_matrix(const ice::math::mat4 &view) = 0;
	/// Set the local-to-world transform used by subsequent legacy draws.
	virtual ice::result set_world_transform(const ice::math::mat4 &world) = 0;

	/// Bind a borrowed texture to a zero-based legacy texture unit; ownership stays with the
	/// texture/device.
	virtual ice::result set_texture(ice::uint index, texture *tex) = 0;

	const ice::math::color &get_ambient_color(void) const;
	winding_cull_mode       get_cull_mode(void) const;

	const ice::math::mat4 &get_ortho(void) const;

	/// CALLBACKS
	virtual void change_size(ice::ushort size_x, ice::ushort size_y) = 0;

	/// Return the most recently measured update rate in updates per second.
	ice::math::real ufps(void) { return ufps_; };
	/// Return the lowest measured update rate in updates per second.
	ice::math::real ulow(void) { return ulow_fps_; };
	/// Return the highest measured update rate in updates per second.
	ice::math::real uhigh(void) { return uhigh_fps_; };

	/// Return the most recently measured render rate in frames per second.
	ice::math::real rfps(void) { return rfps_; };
	/// Return the lowest measured render rate in frames per second.
	ice::math::real rlow(void) { return rlow_fps_; };
	/// Return the highest measured render rate in frames per second.
	ice::math::real rhigh(void) { return rhigh_fps_; };

	/// Return elapsed seconds for the last update interval.
	ice::math::real u_elapsed(void) { return elapsed_; };
	/// Return elapsed seconds for the last render interval.
	ice::math::real r_elapsed(void) { return r_elapsed_; };

	/// Return the configured logical input/window width.
	int get_screen_x(void) { return width_; };
	/// Return the configured logical input/window height.
	int get_screen_y(void) { return height_; };

protected:
	// void*            m_pMainWindow;
	class ice::platform::window *main_window_;

	device_setup device_info_;

	ice::math::color col_clear_;   //! Current clear color
	ice::math::color col_ambient_; //! Current ambient color

	int               height_;
	int               width_;
	winding_cull_mode cullmode_e_; //! Current poly cull mode

	/// PROFILE DATA
	ice::uint64 primitives_drawn_;
	ice::uint64 device_calls_;

	ice::util::timer timer_;
	ice::util::timer r_timer_;

	ice::math::real elapsed_;
	ice::math::real r_elapsed_;

	ice::math::real ufps_;
	ice::math::real uhigh_fps_;
	ice::math::real ulow_fps_;

	ice::math::real rfps_;
	ice::math::real rhigh_fps_;
	ice::math::real rlow_fps_;

	ice::math::real run_counter_;
	ice::math::real run_time_;
	ice::uint64     cur_frame_;

	fog_params fog_params_;

	ice::math::mat4 screen_ortho_;

	bool fog_enable_;

	bool depth_test_;
};

/*! Returns the current ambient color
 *
 *    @returns    const icColor&        The current ambient color
 *
 **/
FORCE_INLINE const ice::math::color &graphics_device::get_ambient_color(void) const {
	return col_ambient_;
}

/*! Returns the current cull mode
 *
 *  @returns    const IC_CULL_MODE    The current cull mode
 **/
FORCE_INLINE winding_cull_mode graphics_device::get_cull_mode(void) const { return cullmode_e_; }

/*! Returns the default ortho matrix
 *
 *  @returns    const icMatrix44&    The current default ortho projection
 **/
FORCE_INLINE const ice::math::mat4 &graphics_device::get_ortho(void) const { return screen_ortho_; }

} // namespace ice::render
