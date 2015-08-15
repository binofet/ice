
#include "PlayerInput.h"

#include "Core/IO/icInput.h"

#include "Airhockey.h"

#include "../Box2D/Box2D.h"


extern Airhockey& gApp;

/*! c'tor
**/
PlayerInput::PlayerInput(int player, b2Body* puck, b2Body* paddle)
    : InputHandler(player, puck, paddle)
{
    m_v2LastMousePos.Set(0.0f,0.0f);
}// END FUNCTION PlayerInput(int player, b2Body* puck, b2Body* paddle)


/*! Processes input and drives a paddle
 *
 *  @param          pInput          Current input
 *  @returns        ICRESULT        Success/failure of input processing
**/
ICRESULT PlayerInput::Process(icInput* pInput)
{
    ICRESULT res = IC_OK;

    bool pressed = false;
    b2Vec2 p(0.0f,0.0f);

    short x,y;
    pInput->GetPos(&x,&y);

    icVector2 mouse_pos((icReal)x,(icReal)y);

    icReal time = m_time.Elapsed();

    // player one input
    if (m_i32Player == PLAYER_ONE)
    {
        icReal dx = pInput->GetAxis(ICAXIS_MOUSE_X) * time * 2000.0f;
        icReal dy = pInput->GetAxis(ICAXIS_MOUSE_Y) * time * 2000.0f;

        p.x += dx;
        p.y -= dy;

        if (!icIsZero(dx) || !icIsZero(dy))
            pressed = true;
    }
    else// player two input
    {
        if (pInput->IsDown(ICKEY_LEFT))
        {
            p.x -= 0.1f;
            pressed = true;
        }
        if (pInput->IsDown(ICKEY_RIGHT))
        {
            p.x += 0.1f;
            pressed = true;
        }
        if (pInput->IsDown(ICKEY_DOWN))
        {
            p.y -= 0.1f;
            pressed = true;
        }
        if (pInput->IsDown(ICKEY_UP))
        {
            p.y += 0.1f;
            pressed = true;
        }
    }

    if (pressed)
        m_Paddle->ApplyForce(p, b2Vec2(0.0f,0.0f));
    else
    {
        float intertia = m_Paddle->GetMass();
        b2Vec2 imp = m_Paddle->GetLinearVelocity();
        imp *= -intertia*0.25f; //! 0.25 is a dampening factor (let it slide to rest)
        m_Paddle->ApplyImpulse(imp, b2Vec2(0.0f,0.0f));
    }

    m_v2LastMousePos = mouse_pos;


    // curved paddle restriction
    // TODO: tune this better
    icVector2 pos(m_Paddle->GetPosition().x, m_Paddle->GetPosition().y);
    if (m_i32Player == PLAYER_ONE)
    {
        static const icVector2 rad_pos(0.0f,-0.7f);
        icReal dist = pos.Distance(rad_pos);
        if (dist > 0.60f)
        {
            icVector2 dir = pos - rad_pos;
            dir.Normalize();
            dir *= 0.60f;
            dir += rad_pos;

            b2Vec2 bpos(dir.x,dir.y);
            m_Paddle->SetXForm(bpos, m_Paddle->GetAngle());

            const float dt = 1.0f/60.0f;
            float intertia = m_Paddle->GetMass();
            b2Vec2 imp = m_Paddle->GetLinearVelocity();
            imp *= -intertia*0.25f;
            m_Paddle->ApplyImpulse(imp, b2Vec2(0.0f,0.0f));
        }
    }
    else
    {
        static const icVector2 rad_pos(0.0f,0.7f);
        icReal dist = pos.Distance(rad_pos);
        if (dist > 0.60f)
        {
            float sx = (float)gApp.GetSettings().iScreen_x;
            float sy = (float)gApp.GetSettings().iScreen_y;

            pInput->WarpCursor((short)sx*0.5f,(short)sy*0.5f);
            icVector2 dir = pos - rad_pos;
            dir.Normalize();
            dir *= 0.60f;
            dir += rad_pos;

            b2Vec2 bpos(dir.x,dir.y);
            m_Paddle->SetXForm(bpos, m_Paddle->GetAngle());

            const float dt = 1.0f/60.0f;
            float intertia = m_Paddle->GetMass();
            b2Vec2 imp = m_Paddle->GetLinearVelocity();
            imp *= -intertia*0.25f;
            m_Paddle->ApplyImpulse(imp, b2Vec2(0.0f,0.0f));
        }
    }

    if (gApp.m_GameState.GameMode == GAME_MODE_CLIENT ||
        gApp.m_GameState.GameMode == GAME_MODE_HOST)
    {
        _airpacket& send = gApp.SendPacket;

        //send.paddle_pos.Set(m_Paddle->GetLinearVelocity().x,
        //                    m_Paddle->GetLinearVelocity().y);

        //send.paddle_pos.Set(p.x,p.y);

        send.paddle_pos.Set(m_Paddle->GetPosition().x,
                            m_Paddle->GetPosition().y);
        send.paddle_vel.Set(m_Paddle->GetLinearVelocity().x,
                            m_Paddle->GetLinearVelocity().y);
    }
    return res;
}// END FUNCTION Process(icInput* pInput)