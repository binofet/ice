#include "PlayerInput.h"
#include "Core/IO/icInput.h"
#include "airhockey.h"
#include "gameplay.h"
extern Airhockey& gApp;
PlayerInput::PlayerInput(int player,b2Body* puck,b2Body* paddle):InputHandler(player,puck,paddle){
    auto p=paddle->GetPosition();m_target.Set(p.x,p.y);
}
void PlayerInput::PrepareFrame(icInput* input){
    if(m_i32Player!=PLAYER_ONE)return;
    b2Vec2 target(m_target.x+input->GetAxis(ICAXIS_MOUSE_X)*1.2f,
                  m_target.y-input->GetAxis(ICAXIS_MOUSE_Y)*1.2f);
    target=gameplay::bounds(target,m_i32Player);m_target.Set(target.x,target.y);
}
ICRESULT PlayerInput::Process(icInput* input,float dt){
    b2Vec2 target(m_target.x,m_target.y);
    if(m_i32Player==PLAYER_TWO){
        target.x+=(input->IsDown(ICKEY_RIGHT)-input->IsDown(ICKEY_LEFT))*1.2f*dt;
        target.y+=(input->IsDown(ICKEY_UP)-input->IsDown(ICKEY_DOWN))*1.2f*dt;
    }
    target=gameplay::bounds(target,m_i32Player);m_target.Set(target.x,target.y);
    gameplay::drive(m_Paddle,target,m_i32Player,dt,2.8f,45.0f);
    if(gApp.m_GameState.GameMode==GAME_MODE_CLIENT||gApp.m_GameState.GameMode==GAME_MODE_HOST){
        auto p=m_Paddle->GetPosition(),v=m_Paddle->GetLinearVelocity();
        gApp.SendPacket.paddle_pos.Set(p.x,p.y);gApp.SendPacket.paddle_vel.Set(v.x,v.y);
    }
    return IC_OK;
}

void PlayerInput::ResetTarget(){auto p=m_Paddle->GetPosition();m_target.Set(p.x,p.y);}
