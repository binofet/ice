
#include "ice/render/graphics_device.h"
namespace ice::render {

/*! Default c'tor
 **/
graphics_device::graphics_device(void) {
	main_window_ = NULL;

	height_    = 0;
	width_     = 0;
	cur_frame_ = 0;

	elapsed_   = 0.0;
	r_elapsed_ = 0.0;

	ufps_      = 0.0;
	uhigh_fps_ = 0.0;
	ulow_fps_  = 10000000.0;

	rfps_      = 0.0;
	rhigh_fps_ = 0.0;
	rlow_fps_  = 100000000.0;

	run_counter_ = 0.0;
	run_time_    = 0.0;

	// Initialize the timers
	timer_.elapsed();
	r_timer_.elapsed();

	depth_test_ = true;
	fog_enable_ = false;
}

/*! Updates frame information
 *
 *      This should be called by objects implenting the icGXDevice
 *      interface
 *
 *  @returns    ICRESULT    Always returns IC_OK
 **/
ice::result graphics_device::update(void) {
	ufps_    = ice::math::real(1) / elapsed_;
	elapsed_ = timer_.elapsed();
	return IC_OK;
} // END FUNTION Update(void)

/*! Ends the current frame
 *
 *      This should be called by objects implenting the icGXDevice
 *      interface
 *
 **/
void graphics_device::frame_end(bool swap) {
	(void)swap;
	r_elapsed_ = r_timer_.elapsed();
	// if (m_RunCounter>=0.0)
	//{
	run_counter_ = 0.0;
	rfps_        = ice::math::real(1.0) / r_elapsed_;
	//}
	run_time_ += r_elapsed_;
	run_counter_ += r_elapsed_;
	// TODO: implement 'high' and 'low' frame values
}
} // namespace ice::render
