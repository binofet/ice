
#include "AiInput.h"

#include "Core/IO/icInput.h"

#include "../Box2D/Box2D.h"

#include "Util/icTime.h"

/*! c'tor
**/
AiInput::AiInput(int player, b2Body* puck, b2Body* paddle)
    : InputHandler(player, puck, paddle)
{
}// END FUNCTION AiInput(int player, b2Body* puck, b2Body* paddle)


/*! Processes input and drives a paddle
 *
 *  @param          pInput          Current input
 *  @returns        ICRESULT        Success/failure of input processing
**/
ICRESULT AiInput::Process(icInput* pInput)
{
    ICRESULT res = IC_OK;

    bool pressed = false;
    b2Vec2 p(0.0f,0.0f);  //! The force the paddle will get
	b2Vec2 puckData(0.0f, 0.0f);
	b2Vec2 AIPaddleData(0.0f,0.0f);

    // player one input
    if (m_i32Player == PLAYER_ONE)
    {
        // AI shouldn't be player one
        return IC_FAIL_GEN;
    }
    else// player two input
    {
		icReal dx = 0.03f;
		icReal dy = 0.03f;
		puckData = m_Puck->GetPosition();
		AIPaddleData = m_Paddle->GetPosition();
		if ( ( puckData.y < 0 ) || ( puckData.y >  AIPaddleData.y ))
		{
			if ( AIPaddleData.x > 0 )
			{
				p.x -= dx;
				p.y += dy;
			}
			else if ( AIPaddleData.x < 0 )
			{
				p.x += dx;
				p.y += dy;
			}
			else if ( AIPaddleData.x == 0 )
			{
				p.x = 0;
				p.y += dy;
			}
		}
		else
		{
			if ( (puckData.x > ( AIPaddleData.x - 0.2 ) ) && (puckData.x < ( AIPaddleData.x + 0.2 ) ) )
			{
				p.x = 0;
				p.y -= dy * 1.5;
			}
			else if ( puckData.x > AIPaddleData.x )
			{
				p.x += dx;
				p.y = 0;
			}
			else if ( puckData.x < AIPaddleData.x )
			{
				p.x -= dx;
				p.y = 0;
			}
		}



		if (!icIsZero(dx) || !icIsZero(dy))
            pressed = true;
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
    return res;
}// END FUNCTION Process(icInput* pInput)