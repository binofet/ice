
#include "paddle.h"

#include "Core/IO/icInput.h"
#include "Content/icContentLoader.h"

#include "../Box2D/Box2D.h"


/*! c'tor
**/
Paddle::Paddle(void)
{
    m_pModel[0] = NULL;
    m_pModel[1] = NULL;
    m_pModel[2] = NULL;

    m_pPhys[0] = NULL;
    m_pPhys[1] = NULL;
    m_pPhys[2] = NULL;

    m_pHandler = NULL;

    player = 0;
}// END FUNCTION Paddle(void)


/*! d'tor
**/
Paddle::~Paddle(void)
{
}// END FUNCTION ~Paddle(void)


/*! Initializes the paddle
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT Paddle::Init(icContentLoader* pContent, b2World* pWorld)
{
    ICRESULT res = IC_OK;
    if (!pContent)
        return IC_FAIL_GEN;

    m_pContent = pContent;

    // load the table model
    res |= m_pContent->Load("Resource/models/paddle.icm",&m_pModel[0]);
    //res |= m_pContent->Load("Resource/models/puck1.icm",&m_pModel[1]);
    //res |= m_pContent->Load("Resource/models/puck2.icm",&m_pModel[2]);

    m_Trans.Identity();
    m_Trans.RotY(IC_PI);
    m_Trans.RotX(-IC_PI/2.0f);

    res |= InitPhysics(pWorld);

    return res;
}// END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)


/*! Updates the paddle
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
**/
ICRESULT Paddle::Update(const float fDeltaTime)
{
    icInput* pInput = icInput::Instance();

    // input handler controls the paddle
    if (m_pHandler)
        m_pHandler->Process(pInput);

    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)


/*! Renders the paddle
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT Paddle::Render(void)
{
    uint cur_paddle=0;

    b2Vec2 p = m_pPhys[cur_paddle]->GetPosition();
    m_Trans.Identity();
    m_Trans.Translate(p.x,-0.055f,p.y);
    m_Trans.RotX(-IC_PI/2.0f);
    if (player == PLAYER_TWO)
        m_Trans.RotZ(-IC_PI);
    
    return m_pModel[cur_paddle]->Render(m_Trans);
}// END FUNCTION Render(void)


/*! Resets physics (kills forces)
**/
void Paddle::Reset(void)
{
    
}// END FUNCTION Reset(void)


/*! Initializes the physics for the paddle
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT Paddle::InitPhysics(b2World* pWorld)
{
    if (!pWorld) return IC_FAIL_GEN;

    // get some of these
	b2BodyDef body_def;
    b2PolygonDef poly_shape_def;
    b2CircleDef circle_shape_def;

    // create the disc paddle physics
    body_def.linearDamping = 0.000001f;
	body_def.position.Set(0.0f, 0.0f);
	m_pPhys[0] = pWorld->CreateBody(&body_def);
    circle_shape_def.radius = 0.05f;
    circle_shape_def.restitution = 0.0f;
	// Set the box density to be non-zero, so it will be dynamic.
	circle_shape_def.density = 1.0f;
	// Override the default friction.
	circle_shape_def.friction = 0.00001f;
	// Add the shape to the body.
	m_pPhys[0]->CreateShape(&circle_shape_def);
	// tell the dynamic body to compute it's mass properties base on its shape.
	m_pPhys[0]->SetMassFromShapes();

    // create the triangle paddle physics

    // create the square paddle physics

    return IC_OK;
}// END FUNCTION InitPhysics(b2World* pWorld)