
#include "NetworkInput.h"

#include "ice/input/input.h"

#include "airhockey.h"

#include "../Box2D/Box2D.h"

extern Airhockey &gApp;

NetworkInput::NetworkInput(int player, b2Body *puck, b2Body *paddle)
    : InputHandler(player, puck, paddle) {}

ice::result NetworkInput::process(ice::input::input *pInput, float dt) {
	ice::result res = IC_OK;

	_airpacket &send = gApp.SendPacket;
	_airpacket &rec  = gApp.ReceivePacket;

	b2Vec2 puck_pos(rec.puck_pos.x, rec.puck_pos.y);
	b2Vec2 paddle_pos(-rec.paddle_pos.x, -rec.paddle_pos.y + 0.15f);
	b2Vec2 paddle_vel(-rec.paddle_vel.x, -rec.paddle_vel.y);

	if (gApp.game_state_.GameMode == GAME_MODE_CLIENT) {
		puck_->SetXForm(puck_pos, rec.puck_ang);

		gApp.game_state_.set_player1_score(rec.score[PLAYER_ONE]);
		gApp.game_state_.set_player2_score(rec.score[PLAYER_TWO]);
	} else {
		(send.puck_pos) = ice::math::vec2(-puck_->GetPosition().x, -puck_->GetPosition().y + 0.15f);
		send.puck_ang   = puck_->GetAngle();

		send.score[PLAYER_ONE] = gApp.game_state_.get_player1_score();
		send.score[PLAYER_TWO] = gApp.game_state_.get_player2_score();
	}

	if (player_ == PLAYER_TWO) {
		paddle_->SetLinearVelocity(paddle_vel);

		paddle_->SetXForm(paddle_pos, 0.0f);
	}

	return res;
}