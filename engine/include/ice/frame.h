#pragma once

#include "ice/widget.h"
namespace ice {

/*! Basic interface for a widget frame
 **/
class frame : public widget {
public:
	frame(ice::assets::content_loader *content);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~frame(void) { cleanup(); };

	virtual result cleanup(void);

	virtual result process_input(ice::input::input *input);
	virtual result update(const math::real delta_seconds);
	virtual result render(void);

	virtual result add_child(widget *out_object, bool relative = false);
	virtual result find_child(widget *out_object);
	virtual result remove_child(widget *out_object);

private:
	widget *head_;

	widget *selected_;
};

} // namespace ice
