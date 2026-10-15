
#include "ice/platform/application.h"
namespace ice::platform {

/*! Default Constructor
 **/
application::application(void) { gx_device_ = NULL; }

/*! Default Destructor
 **/
application::~application(void) { cleanup(); }

/*! Frees application memory
 *
 *    @returns    ICRESULT    Result of Cleanup
 *
 **/
ice::result application::cleanup(void) {
	// if (m_pGxDevice)
	//{
	//     delete m_pGxDevice;
	//     m_pGxDevice = NULL;
	// }
	return IC_OK;
}

ice::result application::init(int argc, char *args[]) {
	argc_  = argc;
	argsz_ = args;
	return IC_OK;
}

/// Pump native events and advance application state; return the application status.
ice::result application::update(void) {
	if (gx_device_) {
		gx_device_->update();
	}
	return IC_OK;
}

/*! Renders the application
 *
 *    @returns    ICRESULT    Result of render
 *
 **/
ice::result application::render(void) {
	if (gx_device_) {
		gx_device_->frame_begin();

		gx_device_->frame_end();
	}
	return IC_OK;
}

/*! TODO: remove this or make it useful
 *
 *    @returns    ICRESULT    Returns IC_OK;
 *
 **/
ice::result application::quit(void) { return IC_OK; }

// void icApplication::OnChangeSize(const icWindow* const pWin,
//                                  ushort sizex, ushort sizey)
//{
//     if (m_pGxDevice)
//     {
//         m_pGxDevice->
//     }
// }
} // namespace ice::platform
