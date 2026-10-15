#ifndef __MAIN_MENU_STATE_H__
#define __MAIN_MENU_STATE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/assets/content_loader.h"
#include "ice/button.h"
#include "ice/frame.h"
#include "ice/util/state_machine.h"
namespace ice::audio {
class sound_i;
}

/*! Implements the games main menu
 **/
class MainMenu : public ice::util::state {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	MainMenu(void);
	~MainMenu(void);

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
	ice::frame  *ui_frame_;
	ice::button *btn_one_player_;
	ice::button *btn_two_player_;
	ice::button *btn_net_play_;
	ice::button *btn_settings_;
	ice::button *btn_exit_;

	ice::audio::sound_i *hover_;
	ice::audio::sound_i *click_;
};

#endif //__MAIN_MENU_STATE_H__