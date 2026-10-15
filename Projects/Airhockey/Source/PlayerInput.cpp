#include "PlayerInput.h"
#include "airhockey.h"
#include "gameplay.h"
#include "ice/input/input.h"
extern Airhockey &gApp;
PlayerInput::PlayerInput(int player, b2Body *puck, b2Body *paddle)
    : InputHandler(player, puck, paddle) {
	auto p    = paddle->GetPosition();
	(target_) = ice::math::vec2(p.x, p.y);
}
void PlayerInput::prepare_frame(ice::input::input *input) {
	if (player_ != PLAYER_ONE)
		return;
	b2Vec2 target(target_.x + input->get_axis(ice::input::mouse_x) * 1.2f,
	              target_.y - input->get_axis(ice::input::mouse_y) * 1.2f);
	target    = gameplay::bounds(target, player_);
	(target_) = ice::math::vec2(target.x, target.y);
}
ice::result PlayerInput::process(ice::input::input *input, float dt) {
	b2Vec2 target(target_.x, target_.y);
	if (player_ == PLAYER_TWO) {
		target.x += (input->is_down(ice::input::key_right) - input->is_down(ice::input::key_left)) *
		            1.2f * dt;
		target.y +=
		    (input->is_down(ice::input::key_up) - input->is_down(ice::input::key_down)) * 1.2f * dt;
	}
	target    = gameplay::bounds(target, player_);
	(target_) = ice::math::vec2(target.x, target.y);
	gameplay::drive(paddle_, target, player_, dt, 2.8f, 45.0f);
	if (gApp.game_state_.GameMode == GAME_MODE_CLIENT ||
	    gApp.game_state_.GameMode == GAME_MODE_HOST) {
		auto p = paddle_->GetPosition(), v = paddle_->GetLinearVelocity();
		(gApp.SendPacket.paddle_pos) = ice::math::vec2(p.x, p.y);
		(gApp.SendPacket.paddle_vel) = ice::math::vec2(v.x, v.y);
	}
	return IC_OK;
}

void PlayerInput::reset_target() {
	auto p    = paddle_->GetPosition();
	(target_) = ice::math::vec2(p.x, p.y);
}
