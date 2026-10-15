
#include "paddle.h"

#include "ice/assets/content_loader.h"
#include "ice/input/input.h"

#include "../Box2D/Box2D.h"

/*! c'tor
 **/
Paddle::Paddle(void) {
	model_[0] = NULL;
	model_[1] = NULL;
	model_[2] = NULL;

	phys_[0] = NULL;
	phys_[1] = NULL;
	phys_[2] = NULL;

	handler_ = NULL;

	player = 0;
} // END FUNCTION Paddle(void)

/*! d'tor
 **/
Paddle::~Paddle(void) {} // END FUNCTION ~Paddle(void)

/*! Initializes the paddle
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result Paddle::init(ice::assets::content_loader *pContent, b2World *pWorld) {
	ice::result res = IC_OK;
	if (!pContent)
		return IC_FAIL_GEN;

	content_ = pContent;

	// load the table model
	res |= content_->load("Resource/models/paddle.icm", &model_[0]);
	// res |= m_pContent->Load("Resource/models/puck1.icm",&m_pModel[1]);
	// res |= m_pContent->Load("Resource/models/puck2.icm",&m_pModel[2]);

	(trans_) = ice::math::mat4(1);
	(trans_) = glm::rotate(trans_, IC_PI, ice::math::vec3(0, 1, 0));
	(trans_) = glm::rotate(trans_, -IC_PI / 2.0f, ice::math::vec3(1, 0, 0));

	res |= init_physics(pWorld);

	return res;
} // END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)

/*! Updates the paddle
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
 **/
ice::result Paddle::update(const float fDeltaTime) {
	ice::input::input *pInput = ice::input::input::instance();

	// input handler controls the paddle
	if (handler_)
		handler_->process(pInput, fDeltaTime);

	return IC_OK;
} // END FUNCTION Update(const float fDeltaTime)

/*! Renders the paddle
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result Paddle::render(void) {
	ice::uint cur_paddle = 0;

	b2Vec2 p = phys_[cur_paddle]->GetPosition();
	(trans_) = ice::math::mat4(1);
	(trans_)[3] += ice::math::vec4(ice::math::vec3(p.x, -0.055f, p.y), 0);
	(trans_) = glm::rotate(trans_, -IC_PI / 2.0f, ice::math::vec3(1, 0, 0));
	if (player == PLAYER_TWO)
		(trans_) = glm::rotate(trans_, -IC_PI, ice::math::vec3(0, 0, 1));

	return model_[cur_paddle]->render(trans_);
} // END FUNCTION Render(void)

/*! Resets physics (kills forces)
 **/
void Paddle::reset(void) {
	if (!phys_[0])
		return;
	phys_[0]->SetXForm(b2Vec2(0, player == PLAYER_ONE ? -0.43f : 0.55f), 0);
	phys_[0]->SetLinearVelocity(b2Vec2(0, 0));
	phys_[0]->SetAngularVelocity(0);
} // END FUNCTION Reset(void)

/*! Initializes the physics for the paddle
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result Paddle::init_physics(b2World *pWorld) {
	if (!pWorld)
		return IC_FAIL_GEN;

	// get some of these
	b2BodyDef    body_def;
	b2PolygonDef poly_shape_def;
	b2CircleDef  circle_shape_def;

	// create the disc paddle physics
	body_def.linearDamping = 0.000001f;
	body_def.position.Set(0.0f, 0.0f);
	phys_[0]                     = pWorld->CreateBody(&body_def);
	circle_shape_def.radius      = 0.05f;
	circle_shape_def.restitution = 0.0f;
	// Set the box density to be non-zero, so it will be dynamic.
	circle_shape_def.density = 1.0f;
	// Override the default friction.
	circle_shape_def.friction = 0.00001f;
	// Add the shape to the body.
	phys_[0]->CreateShape(&circle_shape_def);
	// tell the dynamic body to compute it's mass properties base on its shape.
	phys_[0]->SetMassFromShapes();

	// create the triangle paddle physics

	// create the square paddle physics

	return IC_OK;
} // END FUNCTION InitPhysics(b2World* pWorld)