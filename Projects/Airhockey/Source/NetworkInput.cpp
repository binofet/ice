
#include "NetworkInput.h"

#include "Core/IO/icInput.h"


#include "Airhockey.h"

#include "../Box2D/Box2D.h"

extern Airhockey& gApp;

NetworkInput::NetworkInput(int player, b2Body* puck, b2Body* paddle)
    : InputHandler(player, puck, paddle)
{
}

ICRESULT NetworkInput::Process(icInput* pInput)
{
    ICRESULT res = IC_OK;

    _airpacket& send = gApp.SendPacket;
    _airpacket& rec = gApp.ReceivePacket;

    b2Vec2 puck_pos(rec.puck_pos.x,rec.puck_pos.y);
    b2Vec2 paddle_pos(-rec.paddle_pos.x,-rec.paddle_pos.y+0.15f);
    b2Vec2 paddle_vel(-rec.paddle_vel.x,-rec.paddle_vel.y);

    if (gApp.m_GameState.GameMode == GAME_MODE_CLIENT)
    {
        m_Puck->SetXForm(puck_pos, rec.puck_ang);

        gApp.m_GameState.SetPlayer1Score(rec.score[PLAYER_ONE]);
        gApp.m_GameState.SetPlayer2Score(rec.score[PLAYER_TWO]);
    }
    else
    {
        send.puck_pos.Set(-m_Puck->GetPosition().x,-m_Puck->GetPosition().y+0.15f);
        send.puck_ang = m_Puck->GetAngle();

        send.score[PLAYER_ONE] = gApp.m_GameState.GetPlayer1Score();
        send.score[PLAYER_TWO] = gApp.m_GameState.GetPlayer2Score();
    }

    if (m_i32Player == PLAYER_TWO)
    {     
        m_Paddle->SetLinearVelocity(paddle_vel);
        
        m_Paddle->SetXForm(paddle_pos, 0.0f);
    }

    return res;
}