
#include "ball.h"

#include "ice/assets/content_loader.h"

#include "ice/render/model.h"

#include "btBulletDynamicsCommon.h"

ice::render::model *Ball::m_pBallModel  = NULL;
btCollisionShape   *Ball::m_sphereShape = NULL;

/*! c'tor
 **/
Ball::Ball(void) { in_world_ = false; } // END FUNCTION Ball(void)

/*! d'tor
 **/
Ball::~Ball(void) {} // END FUNCTION ~Ball(void)

/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result Ball::init(ice::assets::content_loader *pContent, btDiscreteDynamicsWorld *pWorld,
                       const ice::math::vec3 &pos) {
	ice::result res = IC_OK;
	if (!pContent)
		return IC_FAIL_GEN;

	content_ = pContent;

	world_ = pWorld;

	// load the Ball model
	if (!m_pBallModel)
		res |= content_->load("resource/models/ball.icm", &m_pBallModel);

	res |= init_physics(pWorld, pos);

	return res;
} // END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)

/*! Updates the Ball
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
 **/
ice::result Ball::update(const float fDeltaTime) {
	if (body_) {
		body_->activate(true);
	}
	return IC_OK;
} // END FUNCTION Update(const float fDeltaTime)

/*! Renders the Ball
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result Ball::render(void) {
	ice::result res = IC_OK;

	ice::math::mat4 trans;

	get_mat(&trans);

	ice::math::vec3 pos = ice::math::vec3((trans)[3]);
	pos.x               = -pos.x;
	ice::math::set_position(trans, ice::math::vec3(pos));
	res = m_pBallModel->render(trans);

	return res;
} // END FUNCTION Render(void)

/*! Initializes the physics for the Ball
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result Ball::init_physics(btDiscreteDynamicsWorld *pWorld, const ice::math::vec3 &pos) {
	if (!pWorld)
		return IC_FAIL_GEN;

	// we only need one of these
	if (!m_sphereShape)
		m_sphereShape = new btSphereShape(3.75f); // 223);

	btVector3 localInertia(0, 0, 0);
	m_sphereShape->calculateLocalInertia(5.0f, localInertia);

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(pos.x, pos.y, pos.z));
	motion_state_ = new btDefaultMotionState(transform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(5.0f, motion_state_, m_sphereShape,
	                                                localInertia);

	rbInfo.m_angularDamping = 0.5f;

	body_ = new btRigidBody(rbInfo);

	return IC_OK;
} // END FUNCTION InitPhysics(b2World* pWorld)

/*! Enable/Disable physics on this ball
 *
 *  @param          bEnable         Toggles physics on/off (true/false)
 **/
void Ball::enable(bool bEnable) {
	if (body_) {
		body_->clearForces();
		if (!in_world_ && bEnable) {
			world_->addRigidBody(body_); //,0x0001,0x000F);
			in_world_ = true;
		} else if (in_world_ && !bEnable) {
			in_world_ = false;
			world_->removeRigidBody(body_);
		}
	}
} // END FUNCTION Enable(bool bEnable)

/*! Set the physics position of the ball
 *
 *  @param          pos         Position to set ball at
 **/
void Ball::set_pos(const ice::math::vec3 &pos) {
	if (body_) {
		body_->clearForces();
		body_->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));

		btTransform trans;

		// m_motionState->getWorldTransform(trans);

		trans = body_->getWorldTransform();

		trans.getOrigin().setX(pos.x);
		trans.getOrigin().setY(pos.y);
		trans.getOrigin().setZ(pos.z);

		// m_motionState->setWorldTransform(trans);

		body_->setWorldTransform(trans);
	}
} // END FUNCTION SetPos(const icVector3& pos)

/*! Returns the physics transform
 *
 *  @param          mat         Address to store transform
 **/
void Ball::get_mat(ice::math::mat4 *mat) {
	btTransform world;
	motion_state_->getWorldTransform(world);

	ice::math::mat4 scaled;
	(scaled)              = ice::math::mat4(1);
	ice::math::real scale = 3.75f / 5.223f;
	(scaled)              = glm::scale(scaled, ice::math::vec3(scale, scale, scale));

	world.getOpenGLMatrix(glm::value_ptr(*mat));

	*mat *= scaled;

} // END FUNCTION GetMat(icMatrix44* mat)