
#include "settings_menu.h"

#include "ice/input/input.h"
#include "ice/render/texture.h"

#include "airhockey.h"

extern Airhockey &gApp;

/*! default c'tor
 **/
SettingsMenu::SettingsMenu(void) {
	ui_frame_ = NULL;

	ptxt_settings_ = NULL;
	ptxt_sfx_      = NULL;
	ptxt_music_    = NULL;
	ptxt_paddle_   = NULL;

	pbtn_back_ = NULL;

	slid_music_vol_ = NULL;
	slid_paddle_    = NULL;
	slid_sfx_vol_   = NULL;

	click_ = NULL;
	hover_ = NULL;

	render_top_only_ = false;
} // END FUNCTION SettingsMenu(void)

/*! default d'tor
 **/
SettingsMenu::~SettingsMenu(void) {} // END FUNCTION ~SettingsMenu(void)

/*! Initializes the main menu state
 *
 *  @param          pContentLoader      Pointer to the content loader
 *  @returns        ICRESULT            Success/failure of initialization
 **/
ice::result SettingsMenu::init(ice::assets::content_loader *pContentLoader) {
	if (!pContentLoader)
		return IC_FAIL_GEN;

	content_ = pContentLoader;

	ice::render::texture *tex1       = NULL;
	ice::render::texture *tex2       = NULL;
	ice::render::texture *bone_theme = NULL;

	ice::audio::sound_param param;
	param.loop  = false;
	param.pitch = 1.0f;
	param.vol   = gApp.get_settings().fSfx_vol;

	// load the textures
	if (ICEFAIL(content_->load("resource/textures/UI_settings_menu.tga", &tex1)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/textures/UI_settings_menu_sel.tga", &tex2)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/textures/UI_bone_theme.tga", &bone_theme)))
		return IC_FAIL_GEN;

	// load the sounds
	if (ICEFAIL(content_->load("resource/audio/whip_01.wav", &hover_)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/audio/weirdbreath.wav", &click_)))
		return IC_FAIL_GEN;

	if (ICEFAIL(content_->load("resource/models/paddle.icm", &paddles_[0])))
		return IC_FAIL_GEN;

	// if (ICEFAIL(m_pContent->Load("resource/models/paddle.icm",&m_Paddles[0])))
	//     return IC_FAIL_GEN;

	// if (ICEFAIL(m_pContent->Load("resource/models/paddle.icm",&m_Paddles[0])))
	//     return IC_FAIL_GEN;

	hover_->set_params(param);
	click_->set_params(param);

	ice::math::real sx = (ice::math::real)gApp.get_settings().iScreen_x;
	ice::math::real sy = (ice::math::real)gApp.get_settings().iScreen_y;

	ice::math::real rat = sy / 1024.0f;
	ice::math::real mx  = sx / 2.0f;

	// set up UI Frame
	ui_frame_ = new ice::frame(content_);

	// m_uiFrame->SetTextures(tex1,tex2);

	ice::math::vec2 bl(0.0f, 0.733f);
	ice::math::vec2 tl(0.0f, 1.0f);
	ice::math::vec2 tr(1.0f, 1.0f);
	ice::math::vec2 br(1.0f, 0.733f);
	// m_uiFrame->SetUVs( tl, tr, bl, br );

	ice::math::vec2  size(sx, sy);
	ice::math::vec2  pos(mx, sy * 0.5f);
	ice::math::color col(0, 0, 0, 255);
	ui_frame_->set_size(size);
	ui_frame_->set_pos(pos);
	ui_frame_->set_color(col);

	// SFX Slider
	slid_sfx_vol_ = new ice::slider(content_);
	slid_sfx_vol_->set_textures(bone_theme, NULL);
	(size) = ice::math::vec2(512.0f * rat, 71.0f * rat);
	(pos)  = ice::math::vec2(mx + mx * 0.35f, sy * 0.609f);
	// col.Set(0,0,0,255);
	slid_sfx_vol_->set_size(size);
	slid_sfx_vol_->set_pos(pos);
	slid_sfx_vol_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 0.138f);
	(tr) = ice::math::vec2(1.0f, 0.138f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	slid_sfx_vol_->set_u_vs(tl, tr, bl, br);
	slid_sfx_vol_->set_high_u_vs(tl, tr, bl, br);
	slid_sfx_vol_->enable_highlight(false);
	slid_sfx_vol_->set_min_max(0, 1000);
	(bl) = ice::math::vec2(0.916f, 0.156f);
	(tl) = ice::math::vec2(0.916f, 0.402f);
	(tr) = ice::math::vec2(1.0f, 0.402f);
	(br) = ice::math::vec2(1.0f, 0.156f);
	slid_sfx_vol_->set_tick_u_vs(tl, tr, bl, br);
	slid_sfx_vol_->set_tick_high_u_vs(tl, tr, bl, br);
	slid_sfx_vol_->set_val((int)(gApp.get_settings().fSfx_vol * 1000.0f));
	(size) = ice::math::vec2(44.0f * rat, 132.0f * rat);
	slid_sfx_vol_->set_tick_size(size);
	ui_frame_->add_child(slid_sfx_vol_);

	// Music Slider
	slid_music_vol_ = new ice::slider(content_);
	slid_music_vol_->set_textures(bone_theme, NULL);
	(size) = ice::math::vec2(512.0f * rat, 71.0f * rat);
	(pos)  = ice::math::vec2(mx + mx * 0.35f, sy * 0.459f);
	// col.Set(0,0,0,255);
	slid_music_vol_->set_size(size);
	slid_music_vol_->set_pos(pos);
	slid_music_vol_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 0.138f);
	(tr) = ice::math::vec2(1.0f, 0.138f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	slid_music_vol_->set_u_vs(tl, tr, bl, br);
	slid_music_vol_->set_high_u_vs(tl, tr, bl, br);
	slid_music_vol_->enable_highlight(false);
	slid_music_vol_->set_min_max(0, 1000);
	(bl) = ice::math::vec2(0.916f, 0.156f);
	(tl) = ice::math::vec2(0.916f, 0.402f);
	(tr) = ice::math::vec2(1.0f, 0.402f);
	(br) = ice::math::vec2(1.0f, 0.156f);
	slid_music_vol_->set_tick_u_vs(tl, tr, bl, br);
	slid_music_vol_->set_tick_high_u_vs(tl, tr, bl, br);
	slid_music_vol_->set_val((int)(gApp.get_settings().fMusic_vol * 1000.0f));
	(size) = ice::math::vec2(44.0f * rat, 132.0f * rat);
	slid_music_vol_->set_tick_size(size);
	ui_frame_->add_child(slid_music_vol_);

	// Paddle Slider
	slid_paddle_ = new ice::slider(content_);
	slid_paddle_->set_textures(bone_theme, NULL);
	(size) = ice::math::vec2(512.0f * rat, 71.0f * rat);
	(pos)  = ice::math::vec2(mx + mx * 0.35f, sy * 0.309f);
	// col.Set(0,0,0,255);
	slid_paddle_->set_size(size);
	slid_paddle_->set_pos(pos);
	slid_paddle_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 0.138f);
	(tr) = ice::math::vec2(1.0f, 0.138f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	slid_paddle_->set_u_vs(tl, tr, bl, br);
	slid_paddle_->set_high_u_vs(tl, tr, bl, br);
	slid_paddle_->enable_highlight(false);
	slid_paddle_->set_min_max(0, 2);
	(bl) = ice::math::vec2(0.916f, 0.156f);
	(tl) = ice::math::vec2(0.916f, 0.402f);
	(tr) = ice::math::vec2(1.0f, 0.402f);
	(br) = ice::math::vec2(1.0f, 0.156f);
	slid_paddle_->set_tick_u_vs(tl, tr, bl, br);
	slid_paddle_->set_tick_high_u_vs(tl, tr, bl, br);
	slid_paddle_->set_val((int)(gApp.get_settings().ucPaddle_type - 1));
	(size) = ice::math::vec2(44.0f * rat, 132.0f * rat);
	slid_paddle_->set_tick_size(size);
	ui_frame_->add_child(slid_paddle_);

	// Settings widget
	ptxt_settings_ = new ice::widget(content_);
	ptxt_settings_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(650.0f * rat, 175.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.799f);
	// col.Set(0,0,0,255);
	ptxt_settings_->set_size(size);
	ptxt_settings_->set_pos(pos);
	ptxt_settings_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.826f);
	(tl) = ice::math::vec2(0.0f, 1.0f);
	(tr) = ice::math::vec2(1.0f, 1.0f);
	(br) = ice::math::vec2(1.0f, 0.826f);
	ptxt_settings_->set_u_vs(tl, tr, bl, br);
	ptxt_settings_->set_high_u_vs(tl, tr, bl, br);
	ptxt_settings_->enable_highlight(false);
	ui_frame_->add_child(ptxt_settings_);

	// SFX Widget
	ptxt_sfx_ = new ice::widget(content_);
	ptxt_sfx_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx - mx * 0.15f, sy * 0.609f);
	// col.Set(0,0,0,255);
	ptxt_sfx_->set_size(size);
	ptxt_sfx_->set_pos(pos);
	ptxt_sfx_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.627f);
	(tl) = ice::math::vec2(0.0f, 0.826f);
	(tr) = ice::math::vec2(1.0f, 0.826f);
	(br) = ice::math::vec2(1.0f, 0.627f);
	ptxt_sfx_->set_u_vs(tl, tr, bl, br);
	ptxt_sfx_->set_high_u_vs(tl, tr, bl, br);
	ptxt_sfx_->enable_highlight(false);
	ui_frame_->add_child(ptxt_sfx_);

	// Music Widget
	ptxt_music_ = new ice::widget(content_);
	ptxt_music_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx - mx * 0.21f, sy * 0.459f);
	// col.Set(0,0,0,255);
	ptxt_music_->set_size(size);
	ptxt_music_->set_pos(pos);
	ptxt_music_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.420f);
	(tl) = ice::math::vec2(0.0f, 0.627f);
	(tr) = ice::math::vec2(1.0f, 0.627f);
	(br) = ice::math::vec2(1.0f, 0.420f);
	ptxt_music_->set_u_vs(tl, tr, bl, br);
	ptxt_music_->set_high_u_vs(tl, tr, bl, br);
	ptxt_music_->enable_highlight(false);
	ui_frame_->add_child(ptxt_music_);

	// Paddle widget
	ptxt_paddle_ = new ice::widget(content_);
	ptxt_paddle_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx - mx * 0.225f, sy * 0.309f);
	// col.Set(0,0,0,255);
	ptxt_paddle_->set_size(size);
	ptxt_paddle_->set_pos(pos);
	ptxt_paddle_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.221f);
	(tl) = ice::math::vec2(0.0f, 0.420f);
	(tr) = ice::math::vec2(1.0f, 0.420f);
	(br) = ice::math::vec2(1.0f, 0.221f);
	ptxt_paddle_->set_u_vs(tl, tr, bl, br);
	ptxt_paddle_->set_high_u_vs(tl, tr, bl, br);
	ptxt_paddle_->enable_highlight(false);
	ui_frame_->add_child(ptxt_paddle_);

	// back button
	pbtn_back_ = new ice::button(content_);
	pbtn_back_->set_textures(tex1, tex2);
	(size) = ice::math::vec2(512.0f * rat, 115.0f * rat);
	(pos)  = ice::math::vec2(mx, sy * 0.109f);
	// col.Set(0,0,0,255);
	pbtn_back_->set_size(size);
	pbtn_back_->set_pos(pos);
	pbtn_back_->set_color(col);
	(bl) = ice::math::vec2(0.0f, 0.0f);
	(tl) = ice::math::vec2(0.0f, 0.221f);
	(tr) = ice::math::vec2(1.0f, 0.221f);
	(br) = ice::math::vec2(1.0f, 0.0f);
	pbtn_back_->set_u_vs(tl, tr, bl, br);
	pbtn_back_->set_high_u_vs(tl, tr, bl, br);
	pbtn_back_->set_sounds(click_, hover_);
	ui_frame_->add_child(pbtn_back_);

	return IC_OK;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Update the main menu
 *
 *  @param          fDeltaTime      Elapsed time since last update
 *  @param          bFinished       Pointer to boolean
 *  @returns        ICRESULT        Success/failure of update
 **/
ice::result SettingsMenu::update(const float fDeltaTime, bool *bFinished) {
	ice::input::input *pInput = ice::input::input::instance();

	AirSettings settings = gApp.get_settings();

	if (pInput->is_pressed(ice::input::key_escape) || pbtn_back_->was_pressed()) {
		pbtn_back_->set_down(false);
		gApp.get_state_machine().pop_state();

		// yep, hack
		ice::util::state *temp = gApp.get_state_machine().pop_state();
		gApp.get_state_machine().push_state(temp);
		return IC_OK;
	}

	if (!ui_frame_)
		return IC_OK;

	ice::result res = ui_frame_->update(fDeltaTime);

	res |= ui_frame_->process_input(pInput);

	settings.fMusic_vol = (float)slid_music_vol_->get_val() / 1000.0f;
	settings.fSfx_vol   = (float)slid_sfx_vol_->get_val() / 1000.0f;

	gApp.update_settings(settings);
	// settings.ucPaddle_type =

	return res;
} // END FUNCTION Update(const float fDeltaTime, bool* bFinished)

/*! Render the main menu
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result SettingsMenu::render(void) {
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

	// temp.Scale(0.5f,0.5f,0.5f);
	// temp.MakeTranslation(200,200.0f,10.0f);
	// m_Paddles[0]->Render(temp);

	return IC_OK;
} // END FUNCTION Render(void)

/*! This gets called when the SettingsMenu gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void SettingsMenu::on_push(void) {
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

void SettingsMenu::on_pop(void) {}