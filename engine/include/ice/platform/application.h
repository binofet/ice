#pragma once

#include "ice/platform/window.h"
#include "ice/render/graphics_device.h"
namespace ice::platform {

/// Legacy desktop application lifecycle; initialize before updating/rendering and clean up on the
/// owning thread.
class application {
public:
	application(void);
	virtual ~application(void);

	virtual ice::result cleanup(void);

	/// Initialize against the supplied borrowed dependencies; inspect the result before using the
	/// subsystem.
	virtual ice::result init(int argc, char *args[]);

	virtual ice::result update(void);
	virtual ice::result render(void);
	virtual ice::result quit(void);

	/// virtual void OnChangeSize(const icWindow* const pWin,
	//    ushort sizex, ushort sizey){};

	/// virtual void OnMinimize(const icWindow* const pWin){};

	/// virtual void OnMaximize(const icWindow* const pWin){};

	/// virtual void OnDrop(const icWindow* const pWin, void* pContext){};

protected:
	ice::render::graphics_device *gx_device_;

	int    argc_;
	char **argsz_;
};

} // namespace ice::platform
