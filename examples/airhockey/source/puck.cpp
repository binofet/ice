
#include "puck.h"
#include "gameplay.h"

#include "ice/assets/content_loader.h"
#include "ice/math/rand.h"

#include "airhockey.h"

#include "../Box2D/Box2D.h"

extern Airhockey &gApp;

/*! d'tor
 **/
Puck::Puck(void) {
	puck_  = NULL;
	phys_  = NULL;
	sound_ = NULL;

	timer_ = 0.0f;
} // END FUNCTON Puck(void)

/*! d'tor
 **/
Puck::~Puck(void) {} // END FUNCTION ~Puck(void)

/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result Puck::init(ice::assets::content_loader *pContent, b2World *pWorld) {
	ice::result res = IC_OK;
	if (!pContent)
		return IC_FAIL_GEN;

	content_ = pContent;

	// load the table model
	res |= content_->load("resource/models/puck.icm", &puck_);

	(trans_) = ice::math::mat4(1);
	(trans_) = glm::rotate(trans_, IC_PI, ice::math::vec3(0, 1, 0));
	(trans_) = glm::rotate(trans_, -IC_PI / 2.0f, ice::math::vec3(1, 0, 0));

	res |= init_physics(pWorld);

	return res;
} // END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)

/*! Updates the puck
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
 **/
ice::result Puck::update(const float fDeltaTime) {
	if (gApp.game_state_.GameMode == GAME_MODE_CLIENT)
		return IC_OK;

	b2Vec2 vel = phys_->GetLinearVelocity();
	phys_->SetLinearVelocity(gameplay::limit(
	    rail_recovery_.update(phys_->GetPosition(), vel, fDeltaTime), gameplay::puckLimit));
	// A quiet puck gets a gentle serve rather than trapping the rally forever.
	if (vel.LengthSquared() < 0.0025f) {
		timer_ += fDeltaTime;
		if (timer_ > 2.5f)
			reset();
	} else
		timer_ = 0;
	return IC_OK;
} // END FUNCTION Update(const float fDeltaTime)

/*! Renders the puck
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result Puck::render(void) {
	b2Vec2 bp = phys_->GetPosition();

	(trans_) = ice::math::mat4(1);
	(trans_)[3] += ice::math::vec4(ice::math::vec3(bp.x, -0.055f, bp.y), 0);
	(trans_) = glm::rotate(trans_, IC_PI / 2.0f, ice::math::vec3(1, 0, 0));
	return puck_->render(trans_);
} // END FUNCTION Render(void)

/*! Resets physics (kills forces)
 **/
void Puck::reset(void) {
	if (puck_) {
		b2Vec2 pos(0.0f, 0.0f);
		phys_->SetXForm(pos, 0.0f);
		phys_->SetAngularVelocity(0.0f);
		float x = ice::math::rand_f(-0.50f, 0.5f);
		float y = ice::math::rand_f(0.0f, 1.0f) < 0.5f ? -0.7f : 0.7f;
		timer_  = 0;
		rail_recovery_.reset();
		phys_->SetLinearVelocity(b2Vec2(x, y));
	}
} // END FUNCTION Reset(void)

/*! Initializes the physics for the puck
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result Puck::init_physics(b2World *pWorld) {
	if (!pWorld)
		return IC_FAIL_GEN;

	// get some of these
	b2BodyDef    body_def;
	b2PolygonDef poly_shape_def;
	b2CircleDef  circle_shape_def;

	// create the puck physics
	body_def.linearDamping = 0.025f;
	body_def.isBullet      = true;
	body_def.position.Set(0.0f, 0.0f);
	phys_ = pWorld->CreateBody(&body_def);
	if (!phys_)
		return IC_FAIL_GEN;
	circle_shape_def.radius = 0.0525f;
	// Set the box density to be non-zero, so it will be dynamic.
	circle_shape_def.density     = 0.1f;
	circle_shape_def.friction    = 0.0f;
	circle_shape_def.restitution = 0.94f;
	// Add the shape to the body.
	phys_->CreateShape(&circle_shape_def);
	// tell the dynamic body to compute it's mass properties base on its shape.
	phys_->SetMassFromShapes();

	b2FilterData filter;
	filter.categoryBits = 0x0002;
	filter.maskBits     = ~0x0004;
	phys_->GetShapeList()->SetFilterData(filter);

	return IC_OK;
} // END FUNCTION InitPhysics(b2World* pWorld)