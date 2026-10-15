#pragma once

#include "ice/globals.h"
#include "ice/math/collision_2d.h"
#include "ice/math/color.h"
#include "ice/math/vec2.h"
namespace ice::render {
class vert_buffer;
}
namespace ice::render {
class texture;
}
namespace ice::input {
class input;
}
namespace ice::assets {
class content_loader;
}
namespace ice {
/*! Abstract interface for all interface objects
 **/
class widget {
public:
	/// Construct with the borrowed dependencies described by this type; initialize explicitly
	/// before active use.
	widget(ice::assets::content_loader *content);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~widget(void) { cleanup(); };

	/// Release resources owned by this subsystem; borrowed dependencies must remain alive until
	/// cleanup completes.
	virtual result cleanup(void) {
		next_ = NULL;
		return IC_OK;
	};

	virtual result process_input(ice::input::input *input);
	virtual result update(const math::real delta_seconds);
	virtual result render(void);

	/// Borrow normal/hover textures for this widget; textures must outlive the widget.
	void set_textures(ice::render::texture *tex1, ice::render::texture *tex2 = NULL);
	/// Borrow the owned texture collection; mutation/reload may invalidate references.
	void get_textures(ice::render::texture **tex1, ice::render::texture **tex2 = NULL);

	/// Set normal texture UV bounds.
	void set_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr, const ice::math::vec2 &bl,
	              const ice::math::vec2 &br);

	/// Set highlighted texture UV bounds.
	void set_high_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
	                   const ice::math::vec2 &bl, const ice::math::vec2 &br);

	/// Set explicit highlight state.
	virtual void set_highlight(bool highlight_on);
	/// Return whether the widget currently renders as highlighted.
	bool is_highlight(void);

	/// Enable automatic hover/highlight processing.
	void enable_highlight(bool enable) { highlight_enabled_ = enable; }

	// bool IsPointerOver(void);

	/// Return/copy the current native cursor/file position in the units defined by this interface.
	const ice::math::vec2 &get_pos(void);
	/// Return/copy the current logical extent or byte size according to the owning interface.
	const ice::math::vec2 &get_size(void);

	/// Borrow the widgets normal display color.
	const ice::math::color &get_color(void);
	/// Borrow the widgets highlighted display color.
	const ice::math::color &get_high_color(void);

	/// Set the cursor/file position using logical coordinates or byte offsets, as specified by the
	/// interface.
	virtual void set_pos(const ice::math::vec2 &pos);
	/// Set logical UI dimensions; dependent geometry is updated by the widget.
	virtual void set_size(const ice::math::vec2 &size);

	/// Set the widgets normal display color.
	virtual void set_color(const ice::math::color &color);
	/// Set the widgets highlighted display color.
	virtual void set_high_color(const ice::math::color &color);

	virtual void update_buffer(void);

	/// Whether the widget preserves its pressed selection state.
	bool sticky;

protected:
	widget *next_;

	ice::assets::content_loader *content_;

	ice::render::vert_buffer *vb_;
	ice::render::texture     *tex1_;
	ice::render::texture     *tex2_;

	ice::math::vec2  pos_;
	ice::math::vec2  size_;
	ice::math::vec2  uvs_[2][4];
	ice::math::color color_;
	ice::math::color high_color_;
	math::real       rotation_;

	int tab_stop_;

	bool highlight_on_;
	bool update_buffer_;
	bool highlight_enabled_;
	bool mouse_over_;

	friend class frame;
};

FORCE_INLINE void widget::set_textures(ice::render::texture *tex1, ice::render::texture *tex2) {
	tex1_ = tex1;
	tex2_ = tex2;
}

FORCE_INLINE void widget::get_textures(ice::render::texture **tex1, ice::render::texture **tex2) {
	if (tex1)
		*tex1 = tex1_;

	if (tex2)
		*tex2 = tex2_;
}

FORCE_INLINE void widget::set_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
                                   const ice::math::vec2 &bl, const ice::math::vec2 &br)

{
	update_buffer_ = true;

	uvs_[0][0] = bl;
	uvs_[0][1] = tl;
	uvs_[0][2] = tr;
	uvs_[0][3] = br;
}
//                     const icVector2& bl, const icVector2& br)

FORCE_INLINE
void widget::set_high_u_vs(const ice::math::vec2 &tl, const ice::math::vec2 &tr,
                           const ice::math::vec2 &bl, const ice::math::vec2 &br)

{
	update_buffer_ = true;

	uvs_[1][0] = bl;
	uvs_[1][1] = tl;
	uvs_[1][2] = tr;
	uvs_[1][3] = br;
}
//                         const icVector2& bl, const icVector2& br)

FORCE_INLINE void widget::set_highlight(bool highlight_on) { highlight_on_ = highlight_on; }

FORCE_INLINE bool widget::is_highlight(void) { return highlight_on_; }

FORCE_INLINE const ice::math::vec2 &widget::get_pos(void) { return pos_; }

FORCE_INLINE const ice::math::vec2 &widget::get_size(void) { return size_; }

FORCE_INLINE const ice::math::color &widget::get_color(void) { return color_; }

FORCE_INLINE const ice::math::color &widget::get_high_color(void) {
	update_buffer_ = true;

	return high_color_;
}

FORCE_INLINE void widget::set_pos(const ice::math::vec2 &pos) { pos_ = pos; }

FORCE_INLINE void widget::set_size(const ice::math::vec2 &size) {
	update_buffer_ = true;
	size_          = size;
}

FORCE_INLINE void widget::set_color(const ice::math::color &color) {
	update_buffer_ = true;
	color_         = color;
}

FORCE_INLINE void widget::set_high_color(const ice::math::color &color) {
	update_buffer_ = true;
	high_color_    = color;
}

} // namespace ice
