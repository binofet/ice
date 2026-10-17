//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "last_game.h"
#include "skeeball.h"

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/input/input.h"
#include "ice/math/rand.h"

extern SkeeBall &gApp;

/*! c'tor
 **/
LastGame::LastGame(void) {
	content_ = NULL;

	render_top_only_ = false;
	update_top_only_ = false;
} // END FUNCTION LastGame(void)

/*! d'tor
 **/
LastGame::~LastGame() {} // END FUNCTION ~LastGame()

/*! Initializes the games state
 *
 *  @param          pContentLoader      Pointer to main content loader
 *  @returns        ICRESULT            Success/failure/warnings of init
 **/
ice::result LastGame::init(ice::assets::content_loader *pContentLoader) {
	ice::result res = IC_OK;

	if (!pContentLoader)
		return IC_FAIL_GEN;

	content_ = pContentLoader;

	ice::render::graphics_device *pDev = content_->get_device();

	(transform_) = ice::math::mat4(1);

	// get us a camera
	ice::math::real ar = (ice::math::real)gApp.get_settings().iScreen_x /
	                     (ice::math::real)gApp.get_settings().iScreen_y;
	max_cam_.get_projection() = glm::perspectiveLH_ZO(float((40.0f * IC_PI / 180.0f)), float(ar),
	                                                  float(1.0f), float(10000000.0f));

	max_cam_.zoom()     = 328.0f;
	max_cam_.zoom_max() = 400.0f;
	max_cam_.zoom_min() = 2.0f;
	max_cam_.xrot()     = 1.5584f;
	max_cam_.yrot()     = 4.683891f;
	(max_cam_.center()) = ice::math::vec3(-7.341785f, 33.721600f, -8.874823f);

	max_cam_.enable_input = gApp.get_settings().bFreeCam;

	int sizex = gApp.get_settings().iScreen_x;
	int sizey = gApp.get_settings().iScreen_y;

	if (gApp.get_settings().bFullscreen) {
		if (sizex == 1920 && sizey == 1080) {
			res |= score_font_.initialize("Arial", 48, content_->get_device());
		} else if (sizex == 1280 && sizey == 720) {
			res |= score_font_.initialize("Arial", 40, content_->get_device());
		} else if (sizex == 1280 && sizey == 800) {
			res |= score_font_.initialize("Arial", 40, content_->get_device());
		} else if (sizex == 1280 && sizey == 1024) {
			res |= score_font_.initialize("Arial", 48, content_->get_device());
		} else if (sizex == 1024 && sizey == 768) {
			res |= score_font_.initialize("Arial", 48, content_->get_device());
		}
	} else {
		if (sizex == 1920 && sizey == 1080) {
			res |= score_font_.initialize("Arial", 48, content_->get_device());
		} else if (sizex == 1280 && sizey == 720) {
			res |= score_font_.initialize("Arial", 40, content_->get_device());
		} else if (sizex == 1280 && sizey == 800) {
			res |= score_font_.initialize("Arial", 40, content_->get_device());
		} else if (sizex == 1280 && sizey == 1024) {
			res |= score_font_.initialize("Arial", 48, content_->get_device());
		} else if (sizex == 1024 && sizey == 768) {
			res |= score_font_.initialize("Arial", 40, content_->get_device());
		}
	}

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	ramp_.init(content_, NULL);

	return res;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Updates the current scores
 **/
void LastGame::update_score() {} // END FUNCTION UpdateScore()

/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
 **/
ice::result LastGame::update(const float fDeltaTime, bool *bFinished) {
	ice::result res = IC_OK;

	ice::input::input *input = ice::input::input::instance();

	// Update the camera
	max_cam_.update(fDeltaTime);

	return res;
} // END FUNCTION Update(const float fDeltaTime, bool *bFinished)

/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
 **/
ice::result LastGame::render(void) {
	ice::result res = IC_OK;

	ice::render::graphics_device *pDev = content_->get_device();

	// set projection and view matrix
	pDev->set_projection(max_cam_.get_projection());
	pDev->set_view_matrix(max_cam_.get_view_mat());

	res |= ramp_.render();

	pDev->set_projection(pDev->get_ortho());
	pDev->set_view_matrix(transform_);
	pDev->set_world_transform(transform_);

	int x_pos = 0.0f;
	int y_pos = 0.0f;

	int sizex = gApp.get_settings().iScreen_x;
	int sizey = gApp.get_settings().iScreen_y;

	if (gApp.get_settings().bFullscreen) {
		if (sizex == 1920 && sizey == 1080) {
			x_pos = gApp.get_settings().iScreen_x * 0.4735f;
			y_pos = gApp.get_settings().iScreen_y * 0.847f;
		}
		if (sizex == 1280 && sizey == 720) {
			x_pos = gApp.get_settings().iScreen_x * 0.5f;
			y_pos = gApp.get_settings().iScreen_y * 0.8f;
		}
		if (sizex == 1280 && sizey == 800) {
			x_pos = gApp.get_settings().iScreen_x * 0.4700f;
			y_pos = gApp.get_settings().iScreen_y * 0.8435f;
		}
		if (sizex == 1280 && sizey == 1024) {
			x_pos = gApp.get_settings().iScreen_x * 0.5f;
			y_pos = gApp.get_settings().iScreen_y * 0.8f;
		}
		if (sizex == 1024 && sizey == 768) {
			x_pos = gApp.get_settings().iScreen_x * 0.5f;
			y_pos = gApp.get_settings().iScreen_y * 0.8f;
		}
	} else {
		if (sizex == 1920 && sizey == 1080) {
			x_pos = gApp.get_settings().iScreen_x * 0.4735f;
			y_pos = gApp.get_settings().iScreen_y * 0.847f;
		}
		if (sizex == 1280 && sizey == 720) {
			x_pos = gApp.get_settings().iScreen_x * 0.4700f;
			y_pos = gApp.get_settings().iScreen_y * 0.8435f;
		}
		if (sizex == 1280 && sizey == 800) {
			x_pos = gApp.get_settings().iScreen_x * 0.4690f;
			y_pos = gApp.get_settings().iScreen_y * 0.8440f;
		}
		if (sizex == 1280 && sizey == 1024) {
			x_pos = gApp.get_settings().iScreen_x * 0.4600f;
			y_pos = gApp.get_settings().iScreen_y * 0.8449f;
		}
		if (sizex == 1024 && sizey == 768) {
			x_pos = gApp.get_settings().iScreen_x * 0.4610f;
			y_pos = gApp.get_settings().iScreen_y * 0.8435f;
		}
	}
	int high = gApp.get_settings().iHighScore;
	score_font_.printf(ice::math::ivec2(x_pos, y_pos), ice::math::color::bred, "%03d", high);

	return res;
} // END FUNCTION Render(void)

/*! This gets called when the gamestate gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void LastGame::on_push(void) {} // END FUNCTION OnPush(void)

/*! This gets called when the gamestate gets popped off the stack
 *  we might not need it
 **/
void LastGame::on_pop(void) {
	// icInput::Instance()->ShowCurs();
} // END FUNCTION OnPop(void)