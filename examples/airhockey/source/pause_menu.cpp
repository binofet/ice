
#include "pause_menu.h"

#include "ice/input/input.h"
#include "ice/render/texture.h"

#include "airhockey.h"

extern Airhockey &gApp;

/*! default c'tor
 **/
PauseMenu::PauseMenu(void) {
	ui_frame_ = NULL;

	wgt_paused_ = NULL;

	btn_main_menu_     = NULL;
	btn_settings_menu_ = NULL;
	btn_back_          = NULL;

	click_ = NULL;
	hover_ = NULL;

	render_top_only_ = false;
	SkipUpdate       = false;
	//    m_bRenderTopOnly = true;
} // END FUNCTION PauseMenu(void)

/*! default d'tor
 **/
PauseMenu::~PauseMenu(void) {} // END FUNCTION ~PauseMenu(void)

/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
 **/
ice::result PauseMenu::init(ice::assets::content_loader *pContentLoader) {
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
	if (ICEFAIL(content_->load("resource/textures/UI_paused_menu.tga", &tex1)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/textures/UI_paused_menu_sel.tga", &tex2)))
		return IC_FAIL_GEN;

	// load the sounds
	if (ICEFAIL(content_->load("resource/audio/whip_01.wav", &hover_)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/audio/weirdbreath.wav", &click_)))
		return IC_FAIL_GEN;

	hover_->set_params(param);
	click_->set_params(param);

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	// set up UI Frame
	ui_frame_ = new ice::frame(content_);

	// m_uiFrame->SetTextures(tex1,tex2);

	ice::math::vec2 bl(0.0f, 0.0f);
	ice::math::vec2 tl(0.0f, 1.0f);
	ice::math::vec2 tr(1.0f, 1.0f);
	ice::math::vec2 br(1.0f, 0.0f);
	ui_frame_->set_u_vs(tl, tr, bl, br);

	ice::math::vec2  size(1280.0f, 720.0f);
	ice::math::vec2  pos(640.0f, 360.0f);
	ice::math::color col(0, 0, 0, 255);
	ui_frame_->set_size(size);
	ui_frame_->set_pos(pos);
	ui_frame_->set_color(col);

	// pause widget
	wgt_paused_ = new ice::widget(content_);
	wgt_paused_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 150.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.809f);
	// col.Set(0,0,0,255);
	wgt_paused_->set_size(size);
	wgt_paused_->set_pos(pos);
	wgt_paused_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.699f);
	(tl) = ice::math::vec2(0.0f, 1.0f);
	(tr) = ice::math::vec2(1.0f, 1.0f);
	(br) = ice::math::vec2(1.0f, 0.699f);
	wgt_paused_->set_u_vs(tl, tr, bl, br);
	wgt_paused_->set_high_u_vs(tl, tr, bl, br);
	wgt_paused_->enable_highlight(false);
	ui_frame_->add_child(wgt_paused_);

	// main menu button
	btn_main_menu_ = new ice::button(content_);
	btn_main_menu_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.571f);
	// col.Set(0,0,0,255);
	btn_main_menu_->set_size(size);
	btn_main_menu_->set_pos(pos);
	btn_main_menu_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.455f);
	(tl) = ice::math::vec2(0.0f, 0.687f);
	(tr) = ice::math::vec2(1.0f, 0.687f);
	(br) = ice::math::vec2(1.0f, 0.455f);
	btn_main_menu_->set_u_vs(tl, tr, bl, br);
	btn_main_menu_->set_high_u_vs(tl, tr, bl, br);
	btn_main_menu_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_main_menu_);

	// settings menu button
	btn_settings_menu_ = new ice::button(content_);
	btn_settings_menu_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 120.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.364f);
	// col.Set(0,0,0,255);
	btn_settings_menu_->set_size(size);
	btn_settings_menu_->set_pos(pos);
	btn_settings_menu_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.209f);
	(tl) = ice::math::vec2(0.0f, 0.424f);
	(tr) = ice::math::vec2(1.0f, 0.424f);
	(br) = ice::math::vec2(1.0f, 0.209f);
	btn_settings_menu_->set_u_vs(tl, tr, bl, br);
	btn_settings_menu_->set_high_u_vs(tl, tr, bl, br);
	btn_settings_menu_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_settings_menu_);

	// back button
	btn_back_ = new ice::button(content_);
	btn_back_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.17f);
	// col.Set(0,0,0,255);
	btn_back_->set_size(size);
	btn_back_->set_pos(pos);
	btn_back_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 0.209f);
	(tr) = ice::math::vec2(1.0f, 0.209f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	btn_back_->set_u_vs(tl, tr, bl, br);
	btn_back_->set_high_u_vs(tl, tr, bl, br);
	btn_back_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_back_);
	return IC_OK;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
 **/
ice::result PauseMenu::update(const float fDeltaTime, bool *bFinished) {
	if (SkipUpdate) {
		SkipUpdate = false;
		return IC_OK;
	}

	ice::input::input *pInput = ice::input::input::instance();

	AirSettings settings = gApp.get_settings();

	if (!btn_back_)
		return IC_OK;

	if (btn_main_menu_->was_pressed()) {
		ice::util::state *state;
		state = gApp.get_state_machine().pop_state();
		state = gApp.get_state_machine().pop_state();
		state = gApp.get_state_machine().pop_state();

		gApp.get_state_machine().push_state(state);

		btn_main_menu_->set_down(false);

		btn_main_menu_->set_down(false);
	} else if (btn_settings_menu_->was_pressed()) {
		gApp.get_state_machine().push_state(&gApp.settings_menu_);
	} else if (pInput->is_pressed(ice::input::key_escape) || btn_back_->was_pressed()) {
		btn_back_->set_down(false);
		gApp.get_state_machine().pop_state();

		// yep, hack
		// icState* temp = gApp.GetStateMachine().PopState();
		// gApp.GetStateMachine().PushState(temp);
		return IC_OK;
	}

	if (!ui_frame_)
		return IC_OK;

	ice::result res = ui_frame_->update(fDeltaTime);

	res |= ui_frame_->process_input(pInput);

	return res;
} // END FUNCTION Update(const float fDeltaTime, bool* bFinished)

/*! Render the main menu
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result PauseMenu::render(void) {
	if (!ui_frame_)
		return IC_OK;

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

/*! This gets called when the PauseMenu gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void PauseMenu::on_push(void) {
	SkipUpdate = true;
	ice::audio::sound_param param;
	// m_bRenderTopOnly = true;
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
} // END FUNCTION OnPush(void)

void PauseMenu::on_pop(void) {
	if (gApp.get_state_machine().peek_state() == &gApp.game_state_)
		ice::input::input::instance()->hide_curs();
}