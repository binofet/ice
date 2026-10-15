#include "AiInput.h"
#include "gameplay.h"
AiInput::AiInput(int player, b2Body *puck, b2Body *paddle) : InputHandler(player, puck, paddle) {
	auto p    = paddle->GetPosition();
	(target_) = ice::math::vec2(p.x, p.y);
}
ice::result AiInput::process(ice::input::input *, float dt) {
	if (player_ != PLAYER_TWO)
		return IC_FAIL_GEN;
	reaction_ -= dt;
	if (reaction_ <= 0) {
		auto target = gameplay::aiTarget(puck_->GetPosition(), puck_->GetLinearVelocity(),
		                                 paddle_->GetPosition());
		(target_)   = ice::math::vec2(target.x, target.y);
		reaction_ += 0.10f;
	}
	gameplay::drive(paddle_, b2Vec2(target_.x, target_.y), player_, dt, 0.95f, 10.0f);
	return IC_OK;
}

void AiInput::reset_target() {
	auto p    = paddle_->GetPosition();
	(target_) = ice::math::vec2(p.x, p.y);
}
