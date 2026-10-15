
#include "net_menu.h"

#include "ice/input/input.h"
#include "ice/render/texture.h"

#include "ice/net/socket.h"

#include "airhockey.h"

extern Airhockey &gApp;

/*! default c'tor
 **/
NetMenu::NetMenu(void) {
	ui_frame_;

	wgt_net_play_;

	btn_host_;
	ice::button *m_btnJoinIP;
	ice::button *m_btnFind;
	ice::button *m_btnJoinFirst;
	ice::button *m_btnMainMenu;

	ice::audio::sound_i *m_pClick;
	ice::audio::sound_i *m_pHover;

	render_top_only_ = false;
} // END FUNCTION NetMenu(void)

/*! default d'tor
 **/
NetMenu::~NetMenu(void) {} // END FUNCTION ~NetMenu(void)

/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
 **/
ice::result NetMenu::init(ice::assets::content_loader *pContentLoader) {
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
	if (ICEFAIL(content_->load("Resource/textures/UI_net_menu.tga", &tex1)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("Resource/textures/UI_net_menu_sel.tga", &tex2)))
		return IC_FAIL_GEN;

	// load the sounds
	if (ICEFAIL(content_->load("Resource/audio/whip_01.wav", &hover_)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("Resource/audio/weirdbreath.wav", &click_)))
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

	// network play widget
	wgt_net_play_ = new ice::widget(content_);
	wgt_net_play_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(650.0f * rat, 135.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.839f);
	// col.Set(0,0,0,255);
	wgt_net_play_->set_size(size);
	wgt_net_play_->set_pos(pos);
	wgt_net_play_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.861f);
	(tl) = ice::math::vec2(0.0f, 1.0f);
	(tr) = ice::math::vec2(1.0f, 1.0f);
	(br) = ice::math::vec2(1.0f, 0.861f);
	wgt_net_play_->set_u_vs(tl, tr, bl, br);
	wgt_net_play_->set_high_u_vs(tl, tr, bl, br);
	wgt_net_play_->enable_highlight(false);
	ui_frame_->add_child(wgt_net_play_);

	// host game button
	btn_host_ = new ice::button(content_);
	btn_host_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.661f);
	// col.Set(0,0,0,255);
	btn_host_->set_size(size);
	btn_host_->set_pos(pos);
	btn_host_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.646f);
	(tl) = ice::math::vec2(0.0f, 0.811f);
	(tr) = ice::math::vec2(1.0f, 0.811f);
	(br) = ice::math::vec2(1.0f, 0.646f);
	btn_host_->set_u_vs(tl, tr, bl, br);
	btn_host_->set_high_u_vs(tl, tr, bl, br);
	btn_host_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_host_);

	// join ip button
	btn_join_ip_ = new ice::button(content_);
	btn_join_ip_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 120.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.504f);
	// col.Set(0,0,0,255);
	btn_join_ip_->set_size(size);
	btn_join_ip_->set_pos(pos);
	btn_join_ip_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.436f);
	(tl) = ice::math::vec2(0.0f, 0.611f);
	(tr) = ice::math::vec2(1.0f, 0.611f);
	(br) = ice::math::vec2(1.0f, 0.436f);
	btn_join_ip_->set_u_vs(tl, tr, bl, br);
	btn_join_ip_->set_high_u_vs(tl, tr, bl, br);
	btn_join_ip_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_join_ip_);

	// find local button
	btn_find_ = new ice::button(content_);
	btn_find_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.33f);
	// col.Set(0,0,0,255);
	btn_find_->set_size(size);
	btn_find_->set_pos(pos);
	btn_find_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.238f);
	(tl) = ice::math::vec2(0.0f, 0.406f);
	(tr) = ice::math::vec2(1.0f, 0.406f);
	(br) = ice::math::vec2(1.0f, 0.238f);
	btn_find_->set_u_vs(tl, tr, bl, br);
	btn_find_->set_high_u_vs(tl, tr, bl, br);
	btn_find_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_find_);

	// main menu button
	btn_main_menu_ = new ice::button(content_);
	btn_main_menu_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 100.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.135f);
	// col.Set(0,0,0,255);
	btn_main_menu_->set_size(size);
	btn_main_menu_->set_pos(pos);
	btn_find_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.039f);
	(tl) = ice::math::vec2(0.0f, 0.189f);
	(tr) = ice::math::vec2(1.0f, 0.189f);
	(br) = ice::math::vec2(1.0f, 0.039f);
	btn_main_menu_->set_u_vs(tl, tr, bl, br);
	btn_main_menu_->set_high_u_vs(tl, tr, bl, br);
	btn_main_menu_->set_sounds(click_, hover_);
	ui_frame_->add_child(btn_main_menu_);
	return IC_OK;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
 **/
ice::result NetMenu::update(const float fDeltaTime, bool *bFinished) {
	ice::input::input *pInput = ice::input::input::instance();

	if (pInput->is_pressed(ice::input::key_escape) || btn_main_menu_->was_pressed()) {
		btn_main_menu_->set_down(false);
		gApp.get_state_machine().pop_state();

		// yep, hack
		ice::util::state *temp = gApp.get_state_machine().pop_state();
		gApp.get_state_machine().push_state(temp);
	} else if (btn_host_->was_pressed()) {
		btn_host_->set_down(false);
		btn_host_->set_down(false);

		gApp.game_state_.GameMode = GAME_MODE_HOST;
		gApp.get_state_machine().pop_state();
		ice::util::state *hack = gApp.get_state_machine().pop_state();
		gApp.get_state_machine().push_state(hack);
		gApp.get_state_machine().push_state(&gApp.game_state_);
	} else if (btn_join_ip_->was_pressed()) {
		// TODO: start the find process, display the games in a selectable list!

		btn_find_->set_down(false);
		btn_find_->set_down(false);

		gApp.game_state_.GameMode = GAME_MODE_CLIENT;
		gApp.get_state_machine().pop_state();
		ice::util::state *hack = gApp.get_state_machine().pop_state();
		gApp.get_state_machine().push_state(hack);
		gApp.get_state_machine().push_state(&gApp.game_state_);
	} else if (btn_find_->was_pressed()) {
		//// TODO: start the find process, display the games in a selectable list!

		// m_btnFind->SetDown(false);
		// m_btnFind->SetDown(false);

		// gApp.m_GameState.GameMode = GAME_MODE_CLIENT;
		// gApp.GetStateMachine().PopState();
		// icState* hack = gApp.GetStateMachine().PopState();
		// gApp.GetStateMachine().PushState(hack);
		// gApp.GetStateMachine().PushState(&gApp.m_GameState);
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
ice::result NetMenu::render(void) {
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

void NetMenu::on_push(void) {}

void NetMenu::on_pop(void) {}