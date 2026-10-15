#ifndef __PAUSE_MENU_STATE_H__
#define __PAUSE_MENU_STATE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/assets/content_loader.h"
#include "ice/button.h"
#include "ice/frame.h"
#include "ice/render/camera.h"
#include "ice/util/state_machine.h"
namespace ice::audio {
class sound_i;
}

/*! Implements the games main menu
 **/
class PauseMenu : public ice::util::state {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	PauseMenu(void);
	~PauseMenu(void);

	ice::result init(ice::assets::content_loader *pContentLoader);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime, bool *bFinished);
	ice::result render(void);

	void on_push(void);
	void on_pop(void);

	bool SkipUpdate;

private:
	ice::assets::content_loader *content_;

	// renderable objects
	ice::frame *ui_frame_;

	ice::widget *wgt_paused_;

	ice::button *btn_main_menu_;
	ice::button *btn_settings_menu_;
	ice::button *btn_back_;

	ice::audio::sound_i *click_;
	ice::audio::sound_i *hover_;
};

#endif //__PAUSE_MENU_STATE_H__