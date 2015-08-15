
#include "ball.h"

#include "Content/icContentLoader.h"

#include "Render/icModel.h"

#include "btBulletDynamicsCommon.h"


icModel* Ball::m_pBallModel = NULL;
btCollisionShape*    Ball::m_sphereShape = NULL;


/*! c'tor
**/
Ball::Ball(void)
{
    m_bInWorld = false;
}// END FUNCTION Ball(void)


/*! d'tor
**/
Ball::~Ball(void)
{
}// END FUNCTION ~Ball(void)


/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT Ball::Init(icContentLoader* pContent, btDiscreteDynamicsWorld* pWorld, const icVector3& pos)
{
    ICRESULT res = IC_OK;
    if (!pContent)
        return IC_FAIL_GEN;

    m_pContent = pContent;

    m_world = pWorld;

    // load the Ball model
    if (!m_pBallModel)
        res |= m_pContent->Load("Resource/models/ball.icm",&m_pBallModel);

    res |= InitPhysics(pWorld,pos);

    return res;
}// END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)


/*! Updates the Ball
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
**/
ICRESULT Ball::Update(const float fDeltaTime)
{
    if (m_body)
    {
        m_body->activate(true);
    }
    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)


/*! Renders the Ball
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT Ball::Render(void)
{
    ICRESULT res = IC_OK;

    icMatrix44 trans;

    GetMat(&trans);

    icVector3 pos = trans.GetPos();
    pos.x = -pos.x;
    trans.SetPos(pos);
    res = m_pBallModel->Render(trans);

    return res;
}// END FUNCTION Render(void)


/*! Initializes the physics for the Ball
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT Ball::InitPhysics(btDiscreteDynamicsWorld* pWorld, const icVector3& pos)
{
    if (!pWorld) return IC_FAIL_GEN;

    // we only need one of these
    if (!m_sphereShape)
        m_sphereShape = new btSphereShape(3.75f);//223);

    btVector3 localInertia(0,0,0);
    m_sphereShape->calculateLocalInertia(5.0f,localInertia);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(pos.x,pos.y,pos.z));
    m_motionState = new btDefaultMotionState(transform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(5.0f,m_motionState,m_sphereShape,localInertia);

    rbInfo.m_angularDamping = 0.5f;

    m_body = new btRigidBody(rbInfo);

    return IC_OK;
}// END FUNCTION InitPhysics(b2World* pWorld)


/*! Enable/Disable physics on this ball
 *
 *  @param          bEnable         Toggles physics on/off (true/false)
**/
void Ball::Enable(bool bEnable)
{
    if (m_body)
    {
        m_body->clearForces();
        if (!m_bInWorld && bEnable)
        {
            m_world->addRigidBody(m_body);//,0x0001,0x000F);
            m_bInWorld = true;
        }
        else if (m_bInWorld && !bEnable)
        {
            m_bInWorld = false;
            m_world->removeRigidBody(m_body);
        }
    }
}// END FUNCTION Enable(bool bEnable)


/*! Set the physics position of the ball
 *
 *  @param          pos         Position to set ball at
**/
void Ball::SetPos(const icVector3& pos)
{
    if (m_body)
    {
        m_body->clearForces();
        m_body->setLinearVelocity(btVector3(0.0f,0.0f,0.0f));

        btTransform trans;

        //m_motionState->getWorldTransform(trans);

        trans = m_body->getWorldTransform();

        trans.getOrigin().setX(pos.x);
        trans.getOrigin().setY(pos.y);
        trans.getOrigin().setZ(pos.z);

        //m_motionState->setWorldTransform(trans);

        m_body->setWorldTransform(trans);
    }
}// END FUNCTION SetPos(const icVector3& pos)


/*! Returns the physics transform
 *
 *  @param          mat         Address to store transform
**/
void Ball::GetMat(icMatrix44* mat)
{
    btTransform world;
    m_motionState->getWorldTransform(world);

    icMatrix44 scaled;
    scaled.Identity();
    icReal scale = 3.75f/5.223f;
    scaled.Scale(scale,scale,scale);

    world.getOpenGLMatrix((btScalar*)mat);

    *mat *= scaled;


}// END FUNCTION GetMat(icMatrix44* mat)