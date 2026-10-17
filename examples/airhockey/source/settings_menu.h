#ifndef __SETTINGS_MENU_STATE_H__
#define __SETTINGS_MENU_STATE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/assets/content_loader.h"
#include "ice/button.h"
#include "ice/frame.h"
#include "ice/render/camera.h"
#include "ice/slider.h"
#include "ice/util/state_machine.h"
namespace ice {
class slider;
}

/*! Implements the games main menu
 **/
class SettingsMenu : public ice::util::state {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	SettingsMenu(void);
	~SettingsMenu(void);

	ice::result init(ice::assets::content_loader *pContentLoader);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime, bool *bFinished);
	ice::result render(void);

	void on_push(void);
	void on_pop(void);

private:
	ice::assets::content_loader *content_;

	// renderable objects
	ice::frame  *ui_frame_;
	ice::widget *ptxt_settings_;
	ice::widget *ptxt_sfx_;
	ice::widget *ptxt_music_;
	ice::widget *ptxt_paddle_;

	ice::button *pbtn_back_;

	ice::slider *slid_sfx_vol_;
	ice::slider *slid_music_vol_;
	ice::slider *slid_paddle_;

	ice::audio::sound_i *click_;
	ice::audio::sound_i *hover_;

	ice::render::model *paddles_[3];
};

#endif //__SETTINGS_MENU_STATE_H__