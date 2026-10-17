#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__
#include <cstdint>

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

#define GAME_MODE_SINGLE 0
#define GAME_MODE_TWO 1
#define GAME_MODE_HOST 2
#define GAME_MODE_CLIENT 3

#define WINING_SCORE 10
#define WIN_STALL_TIMER 3.0f

#define MAX_BALLS 5

#define MAX_BALLS_OUT 3

/*! This is the main game state
 **/
class GameState : public ice::util::state {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	GameState(void);
	~GameState(void);

	ice::result init(ice::assets::content_loader *pContentLoader);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime, bool *bFinished);
	ice::result render(void);

	void on_push(void);
	void on_pop(void);

	int  GameMode;
	bool ready_for_shot() const {
		return bWaiting && !bInPlay && cur_ball_ > 0 && cur_ball_ <= MAX_BALLS;
	}
	bool shot_in_flight() const { return bInPlay; }

private:
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	ice::result init_physics(void);

	void update_physics(const float fDeltaTime);

	void update_score(const float fDeltaTime);

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE MEMBERS

	// cameras
	ice::render::max_cam max_cam_;

	ice::math::mat4 transform_;
	ice::math::mat4 m44_tickets_;

	ice::assets::content_loader *content_;

	// game objects
	ice::render::gl::font_gl font_;
	ice::render::gl::font_gl score_font_;

	ice::render::model *ticket_start_;
	ice::render::model *ticket_roll_;

	ice::math::vec3 accel_;

	float dy_;
	float ticket_rot_;
	float percent_;
	float game_over_timer_;
	float play_timer_;

	int score_;
	int num_tickets_;

	bool last_lmb_;
	bool bScored;

	std::uint64_t companion_shooter_    = 0;
	unsigned      companion_shot_score_ = 0;
	bool          companion_settled_    = false;
	bool          bInPlay;
	bool          bWaiting;

	Ball balls_[MAX_BALLS];
	int  cur_ball_;
	Ramp ramp_;

	ice::audio::sound_i *score_snd_;
	ice::audio::sound_i *loser_snd_;
	ice::audio::sound_i *score2nd_;

	// physics
	btDiscreteDynamicsWorld             *phys_world_;
	btCollisionDispatcher               *dispatcher_;
	btAxisSweep3                        *pair_cache_;
	btSequentialImpulseConstraintSolver *constraint_solver_;
	btDefaultCollisionConfiguration     *collision_config_;
};

#endif //__GAME_STATE_H__