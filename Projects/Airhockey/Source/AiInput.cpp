#include "AiInput.h"
#include "gameplay.h"
AiInput::AiInput(int player,b2Body* puck,b2Body* paddle):InputHandler(player,puck,paddle){
    auto p=paddle->GetPosition();m_target.Set(p.x,p.y);
}
ICRESULT AiInput::Process(icInput*,float dt){
    if(m_i32Player!=PLAYER_TWO)return IC_FAIL_GEN;
    m_reaction-=dt;
    if(m_reaction<=0){
        auto target=gameplay::aiTarget(m_Puck->GetPosition(),m_Puck->GetLinearVelocity(),m_Paddle->GetPosition());
        m_target.Set(target.x,target.y);m_reaction+=0.10f;
    }
    gameplay::drive(m_Paddle,b2Vec2(m_target.x,m_target.y),m_i32Player,dt,0.95f,10.0f);
    return IC_OK;
}

void AiInput::ResetTarget(){auto p=m_Paddle->GetPosition();m_target.Set(p.x,p.y);}
