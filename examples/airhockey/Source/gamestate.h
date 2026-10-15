#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/assets/content_loader.h"
#include "ice/render/camera.h"
#include "ice/render/chase_cam.h"
#include "ice/render/font.h"
#include "ice/render/index_buf.h"
#include "ice/render/model.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"
#include "ice/util/state_machine.h"
#include "ice/widget.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "paddle.h"
#include "puck.h"
#include "table.h"

#include "../Box2D/Box2D.h"

#define GAME_MODE_SINGLE 0
#define GAME_MODE_TWO 1
#define GAME_MODE_HOST 2
#define GAME_MODE_CLIENT 3

#define WINING_SCORE 10
#define WIN_STALL_TIMER 3.0f

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

	int GameMode;

	int get_player1_score() { return score_[PLAYER_ONE]; };
	int get_player2_score() { return score_[PLAYER_TWO]; };

	void set_player1_score(int s) { score_[PLAYER_ONE] = s; };
	void set_player2_score(int s) { score_[PLAYER_TWO] = s; };

private:
	friend struct GameplaySmokeChecks;
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	ice::result create_ground(void);
	ice::result init_physics(void);
	ice::result update_u_vs(const float fDeltaTime);

	void update_physics(const float fDeltaTime);

	void update_score();

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE MEMBERS

	// cameras
	ice::render::chase_cam chase_cam_;
	ice::render::camera    camera_;

	ice::math::mat4 transform_;

	ice::assets::content_loader *content_;

	// ground
	ice::render::texture     *ground_tex_;
	ice::render::vert_buffer *ground_;
	ice::render::index_buf   *ground_ib_;

	// game objects
	ice::render::font *font_;
	Table              table_;
	Puck               puck_;       //! Puck
	Paddle             paddles_[2]; //! Paddles

	ice::widget          *wgt_win_;
	ice::render::texture *tex_plyr1_;
	ice::render::texture *tex_plyr2_;
	ice::widget          *wgt_score_[2];
	int                   score_[2];
	float                 win_timer_;

	ice::audio::sound_i *score_snd_;
	ice::audio::sound_i *loser_snd_;

	// physics
	b2World *world_;
	float    serve_timer_   = 0;
	float    accumulator_   = 0;
	unsigned physics_steps_ = 0;

public:
	unsigned physics_steps() const { return physics_steps_; }
};

#endif //__GAME_STATE_H__