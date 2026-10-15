
#include "main_menu.h"

#include "ice/input/input.h"
#include "ice/render/texture.h"

#include "airhockey.h"

extern Airhockey &gApp;

/*! default c'tor
 **/
MainMenu::MainMenu(void) {
	ui_frame_       = NULL;
	btn_one_player_ = NULL;
	btn_two_player_ = NULL;
	btn_net_play_   = NULL;
	btn_settings_   = NULL;
	btn_exit_       = NULL;

	hover_ = NULL;
	click_ = NULL;

	render_top_only_ = true;
	SkipUpdate       = false;
} // END FUNCTION MainMenu(void)

/*! default d'tor
 **/
MainMenu::~MainMenu(void) {} // END FUNCTION ~MainMenu(void)

/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
 **/
ice::result MainMenu::init(ice::assets::content_loader *pContentLoader) {
	if (!pContentLoader)
		return IC_FAIL_GEN;

	content_ = pContentLoader;

	ice::render::texture *tex1 = NULL;
	ice::render::texture *tex2 = NULL;

	ice::audio::sound_param param;
	param.loop  = false;
	param.pitch = 1.0f;
	param.vol   = gApp.get_settings().fSfx_vol;

	// load the textures
	if (ICEFAIL(content_->load("Resource/textures/UI_mainmenu.tga", &tex1)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("Resource/textures/UI_mainmenu_sel.tga", &tex2)))
		return IC_FAIL_GEN;

	// load the sounds
	if (ICEFAIL(content_->load("Resource/audio/whip_01.wav", &hover_)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("Resource/audio/weirdbreath.wav", &click_)))
		return IC_FAIL_GEN;

	hover_->set_params(param);
	click_->set_params(param);

	// set up UI Frame
	ui_frame_ = new ice::frame(content_);

	// m_uiFrame->SetTextures(tex1,tex2);

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	ice::math::vec2 bl(0.0f, 0.0f);
	ice::math::vec2 tl(0.0f, 1.0f);
	ice::math::vec2 tr(1.0f, 1.0f);
	ice::math::vec2 br(1.0f, 0.0f);
	ui_frame_->set_u_vs(tl, tr, bl, br);

	ice::math::vec2  size(sx, sy);
	ice::math::vec2  pos(mx, 360.0f);
	ice::math::color col(0, 0, 0, 0);
	ui_frame_->set_size(size);
	ui_frame_->set_pos(pos);
	ui_frame_->set_color(col);

	// add the buttons to the UI frame
	// one player button
	btn_one_player_ = new ice::button(content_);
	btn_one_player_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 137.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.863f);
	col.set(0, 0, 0, 255);
	btn_one_player_->set_size(size);
	btn_one_player_->set_pos(pos);
	btn_one_player_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.857f);
	(tl) = ice::math::vec2(0.0f, 1.0f);
	(tr) = ice::math::vec2(1.0f, 1.0f);
	(br) = ice::math::vec2(1.0f, 0.857f);
	btn_one_player_->set_u_vs(tl, tr, bl, br);
	btn_one_player_->set_high_u_vs(tl, tr, bl, br);
	btn_one_player_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_one_player_);

	// two player button
	btn_two_player_ = new ice::button(content_);
	btn_two_player_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 157.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.709f);
	// col.Set(0,0,0,255);
	btn_two_player_->set_size(size);
	btn_two_player_->set_pos(pos);
	btn_two_player_->set_color(col);
	(tl) = ice::math::vec2(0.0f, 0.807f);
	(tr) = ice::math::vec2(1.0f, 0.807f);
	(bl) = ice::math::vec2(0.0f, 0.669f);
	(br) = ice::math::vec2(1.0f, 0.669f);
	btn_two_player_->set_u_vs(tl, tr, bl, br);
	btn_two_player_->set_high_u_vs(tl, tr, bl, br);
	btn_two_player_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_two_player_);

	// network play button
	btn_net_play_ = new ice::button(content_);
	btn_net_play_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 248.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.5f);
	// col.Set(0,0,0,255);
	btn_net_play_->set_size(size);
	btn_net_play_->set_pos(pos);
	btn_net_play_->set_color(col);
	(tl) = ice::math::vec2(0.0f, 0.669f);
	(tr) = ice::math::vec2(1.0f, 0.669f);
	(bl) = ice::math::vec2(0.0f, 0.385f);
	(br) = ice::math::vec2(1.0f, 0.385f);
	btn_net_play_->set_u_vs(tl, tr, bl, br);
	btn_net_play_->set_high_u_vs(tl, tr, bl, br);
	btn_net_play_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_net_play_);

	// settings button
	btn_settings_ = new ice::button(content_);
	btn_settings_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 128.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.257f);
	// col.Set(0,0,0,255);
	btn_settings_->set_size(size);
	btn_settings_->set_pos(pos);
	btn_settings_->set_color(col);
	(tl) = ice::math::vec2(0.0f, 0.322f);
	(tr) = ice::math::vec2(1.0f, 0.322f);
	(bl) = ice::math::vec2(0.0f, 0.163f);
	(br) = ice::math::vec2(1.0f, 0.163f);
	btn_settings_->set_u_vs(tl, tr, bl, br);
	btn_settings_->set_high_u_vs(tl, tr, bl, br);
	btn_settings_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_settings_);

	// exit button
	btn_exit_ = new ice::button(content_);
	btn_exit_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 125.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.093f);
	// col.Set(0,0,0,255);
	btn_exit_->set_size(size);
	btn_exit_->set_pos(pos);
	btn_exit_->set_color(col);
	(tl) = ice::math::vec2(0.0f, 0.123f);
	(tr) = ice::math::vec2(1.0f, 0.123f);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	btn_exit_->set_u_vs(tl, tr, bl, br);
	btn_exit_->set_high_u_vs(tl, tr, bl, br);
	btn_exit_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_exit_);

	return IC_OK;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
 **/
ice::result MainMenu::update(const float fDeltaTime, bool *bFinished) {
	if (SkipUpdate) {
		btn_one_player_->set_down(false);
		btn_two_player_->set_down(false);
		btn_settings_->set_down(false);
		btn_net_play_->set_down(false);
		SkipUpdate = false;
		return IC_OK;
	}

	ice::result res = ui_frame_->update(fDeltaTime);

	ice::input::input *pInput = ice::input::input::instance();

	res |= ui_frame_->process_input(pInput);

	if (btn_one_player_->was_pressed()) {
		btn_one_player_->set_down(false);
		btn_one_player_->set_down(false);

		gApp.game_state_.GameMode = GAME_MODE_SINGLE;
		gApp.get_state_machine().push_state(&gApp.game_state_);
	} else if (btn_two_player_->was_pressed()) {
		btn_two_player_->set_down(false);
		btn_two_player_->set_down(false);

		gApp.game_state_.GameMode = GAME_MODE_TWO;
		gApp.get_state_machine().push_state(&gApp.game_state_);
	} else if (btn_net_play_->was_pressed()) {
		render_top_only_ = false;
		gApp.get_state_machine().push_state(&gApp.net_menu_);
	} else if (btn_settings_->was_pressed()) {
		render_top_only_ = false;
		// m_btnSettings->SetDown(false);
		gApp.get_state_machine().push_state(&gApp.settings_menu_);
	} else if (pInput->is_pressed(ice::input::key_escape) || btn_exit_->was_pressed()) {
		// HACK
		gApp.quit();
	}

	return res;
} // END FUNCTION Update(const float fDeltaTime, bool* bFinished)

/*! Render the main menu
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result MainMenu::render(void) {
	ice::render::graphics_device *pDev = content_->get_device();

	ice::math::mat4 temp;
	(temp) = ice::math::mat4(1);
	// set projection and view matrix
	pDev->set_projection(pDev->get_ortho());
	pDev->set_view_matrix(temp);

	// render the UI
	ui_frame_->render();

	return IC_OK;
} // END FUNCTION Render(void)

/*! This gets called when the MainMenu gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void MainMenu::on_push(void) {
	ice::audio::sound_param param;
	render_top_only_ = true;
	if (hover_) {
		param     = hover_->get_params();
		param.vol = gApp.get_settings().fSfx_vol;
		hover_->set_params(param);
	}
	if (click_) {
		param     = click_->get_params();
		param.vol = gApp.get_settings().fSfx_vol;
		click_->set_params(param);
	}

	SkipUpdate = true;
} // END FUNCTION OnPush(void)

/*! blas
 **/
void MainMenu::on_pop(void) { render_top_only_ = true; } // END FUNCTION OnPop(void)
