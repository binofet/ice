#ifndef __LAST_GAME_H__
#define __LAST_GAME_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/assets/content_loader.h"
#include "ice/render/gl/font_gl.h"
#include "ice/render/index_buf.h"
#include "ice/render/max_cam.h"
#include "ice/render/model.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"
#include "ice/util/state_machine.h"
#include "ice/widget.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "ball.h"
#include "btBulletDynamicsCommon.h"
#include "skeeball_ramp.h"

/*! This is the main game state
 **/
class LastGame : public ice::util::state {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	LastGame(void);
	~LastGame(void);

	ice::result init(ice::assets::content_loader *pContentLoader);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime, bool *bFinished);
	ice::result render(void);

	void on_push(void);
	void on_pop(void);

	int GameMode;

private:
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	ice::result init_physics(void);

	void update_physics(const float fDeltaTime);

	void update_score();

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE MEMBERS

	// cameras
	ice::render::max_cam max_cam_;

	ice::math::mat4 transform_;

	ice::assets::content_loader *content_;

	// game objects
	ice::render::gl::font_gl score_font_;

	Ramp ramp_;
};

#endif //__LAST_GAME_H__