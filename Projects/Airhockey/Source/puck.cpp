
#include "puck.h"
#include "gameplay.h"

#include "Math/icRand.h"
#include "Content/icContentLoader.h"

#include "airhockey.h"

#include "../Box2D/Box2D.h"

extern Airhockey& gApp;


/*! d'tor
**/
Puck::Puck(void)
{
    m_pPuck = NULL;
    m_pPhys = NULL;
    m_pSound = NULL;

    m_fTimer = 0.0f;
}// END FUNCTON Puck(void)


/*! d'tor
**/
Puck::~Puck(void)
{
}// END FUNCTION ~Puck(void)


/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT Puck::Init(icContentLoader* pContent, b2World* pWorld)
{
    ICRESULT res = IC_OK;
    if (!pContent)
        return IC_FAIL_GEN;

    m_pContent = pContent;

    // load the table model
    res |= m_pContent->Load("Resource/models/puck.icm",&m_pPuck);

    m_Trans.Identity();
    m_Trans.RotY(IC_PI);
    m_Trans.RotX(-IC_PI/2.0f);

    res |= InitPhysics(pWorld);

    return res;
}// END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)


/*! Updates the puck
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
**/
ICRESULT Puck::Update(const float fDeltaTime)
{
    if (gApp.m_GameState.GameMode == GAME_MODE_CLIENT)
        return IC_OK;

    b2Vec2 vel=m_pPhys->GetLinearVelocity();
    m_pPhys->SetLinearVelocity(gameplay::limit(m_railRecovery.update(m_pPhys->GetPosition(),vel,fDeltaTime),gameplay::puckLimit));
    // A quiet puck gets a gentle serve rather than trapping the rally forever.
    if(vel.LengthSquared()<0.0025f){
        m_fTimer+=fDeltaTime;
        if(m_fTimer>2.5f)Reset();
    }else m_fTimer=0;
    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)


/*! Renders the puck
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT Puck::Render(void)
{
    b2Vec2 bp = m_pPhys->GetPosition();

    m_Trans.Identity();
    m_Trans.Translate(bp.x,-0.055f,bp.y);
    m_Trans.RotX(IC_PI/2.0f);
    return m_pPuck->Render(m_Trans);
}// END FUNCTION Render(void)


/*! Resets physics (kills forces)
**/
void Puck::Reset(void)
{
    if (m_pPuck)
    {
        b2Vec2 pos(0.0f,0.0f);
        m_pPhys->SetXForm(pos,0.0f);
        m_pPhys->SetAngularVelocity(0.0f);
        float x = icRandF(-0.50f,0.5f);
        float y = icRandF(0.0f,1.0f)<0.5f?-0.7f:0.7f;
        m_fTimer=0;
        m_railRecovery.reset();
        m_pPhys->SetLinearVelocity(b2Vec2(x,y));
    }
}// END FUNCTION Reset(void)


/*! Initializes the physics for the puck
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT Puck::InitPhysics(b2World* pWorld)
{
    if (!pWorld) return IC_FAIL_GEN;

    // get some of these
	b2BodyDef body_def;
    b2PolygonDef poly_shape_def;
    b2CircleDef circle_shape_def;

    // create the puck physics
    body_def.linearDamping = 0.025f;
    body_def.isBullet = true;
	body_def.position.Set(0.0f, 0.0f);
	m_pPhys = pWorld->CreateBody(&body_def);
    if (!m_pPhys) return IC_FAIL_GEN;
    circle_shape_def.radius = 0.0525f;
	// Set the box density to be non-zero, so it will be dynamic.
	circle_shape_def.density = 0.1f;
	circle_shape_def.friction = 0.0f;
    circle_shape_def.restitution = 0.94f;
	// Add the shape to the body.
	m_pPhys->CreateShape(&circle_shape_def);
	// tell the dynamic body to compute it's mass properties base on its shape.
	m_pPhys->SetMassFromShapes();

    b2FilterData filter;
    filter.categoryBits = 0x0002;
    filter.maskBits = ~0x0004;
    m_pPhys->GetShapeList()->SetFilterData(filter);

    return IC_OK;
}// END FUNCTION InitPhysics(b2World* pWorld)