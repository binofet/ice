
#include "table.h"

#include "ice/assets/content_loader.h"

#include "../Box2D/Box2D.h"

/*! c'tor
 **/
Table::Table(void) {
	table_  = NULL;
	table2_ = NULL;
	table3_ = NULL;
	table4_ = NULL;

	wall1_ = NULL;
	wall2_ = NULL;
	wall3_ = NULL;
	wall4_ = NULL;
	wall5_ = NULL;
	wall6_ = NULL;
	wall7_ = NULL;
	wall8_ = NULL;
} // END FUNCTION Table(void)

/*! d'tor
 **/
Table::~Table(void) {} // END FUNCTION ~Table(void)

/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result Table::init(ice::assets::content_loader *pContent, b2World *pWorld) {
	ice::result res = IC_OK;
	if (!pContent)
		return IC_FAIL_GEN;

	content_ = pContent;

	// load the table model
	res |= content_->load("resource/models/table.icm", &table_);
	res |= content_->load("resource/models/table_2.icm", &table2_);
	// res |= m_pContent->Load("resource/models/table_3.icm",&m_pTable3);
	// res |= m_pContent->Load("resource/models/table_4.icm",&m_pTable4);

	(trans_) = ice::math::mat4(1);
	(trans_) = glm::rotate(trans_, IC_PI, ice::math::vec3(0, 1, 0));
	(trans_) = glm::rotate(trans_, -IC_PI / 2.0f, ice::math::vec3(1, 0, 0));

	res |= init_physics(pWorld);

	return res;
} // END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)

/*! Updates the table
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
 **/
ice::result Table::update(const float fDeltaTime) {
	return IC_OK;
} // END FUNCTION Update(const float fDeltaTime)

/*! Renders the table
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result Table::render(void) {
	ice::result res = IC_OK;
	res             = table_->render(trans_);
	res |= table2_->render(trans_);
	// m_pTable3->Render(m_Trans);
	// m_pTable4->Render(m_Trans);
	return res;
} // END FUNCTION Render(void)

/*! Initializes the physics for the table
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result Table::init_physics(b2World *pWorld) {
	if (!pWorld)
		return IC_FAIL_GEN;

	// get some of these
	b2BodyDef    body_def;
	b2PolygonDef poly_shape_def;

	// create wall 1
	body_def.position.Set(-1.15f / 3.0f, -0.835f);
	wall1_ = pWorld->CreateBody(&body_def);
	if (!wall1_)
		return IC_FAIL_GEN;
	// set restitution
	poly_shape_def.restitution = 0.94f;
	poly_shape_def.friction    = 0.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	wall1_->CreateShape(&poly_shape_def);

	// create wall 2
	body_def.position.Set(-1.15f / 3.0f, 0.95f);
	wall2_ = pWorld->CreateBody(&body_def);
	if (!wall2_)
		return IC_FAIL_GEN;
	// poly_shape_def.restitution = 1.0f;
	//  The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	wall2_->CreateShape(&poly_shape_def);

	// create wall 3
	body_def.position.Set(-0.55f, 0.0f);
	wall3_ = pWorld->CreateBody(&body_def);
	if (!wall3_)
		return IC_FAIL_GEN;
	// poly_shape_def.restitution = 1.0f;
	//  The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.2f, 2.025f);
	// Add the ground shape to the ground body.
	wall3_->CreateShape(&poly_shape_def);

	// create wall 4
	body_def.position.Set(1.0f / 3.0f, 0.95f);
	wall4_ = pWorld->CreateBody(&body_def);
	if (!wall4_)
		return IC_FAIL_GEN;
	// poly_shape_def.restitution = 1.0f;
	//  The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	wall4_->CreateShape(&poly_shape_def);

	// create wall 5
	body_def.position.Set(0.55f, 0.0f);
	wall5_ = pWorld->CreateBody(&body_def);
	if (!wall5_)
		return IC_FAIL_GEN;
	// poly_shape_def.restitution = 1.0f;
	//  The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.2f, 2.025f);
	// Add the ground shape to the ground body.
	wall5_->CreateShape(&poly_shape_def);

	// create wall 6
	body_def.position.Set(1.0f / 3.0f, -0.835f);
	wall6_ = pWorld->CreateBody(&body_def);
	if (!wall6_)
		return IC_FAIL_GEN;
	// set restitution
	poly_shape_def.restitution = 0.94f;
	poly_shape_def.friction    = 0.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	wall6_->CreateShape(&poly_shape_def);

	// create wall 7
	b2FilterData filter;
	filter.categoryBits = 0x0001;
	filter.maskBits     = ~0x0002;
	body_def.position.Set(0.0f, -0.835f);
	wall7_ = pWorld->CreateBody(&body_def);
	if (!wall7_)
		return IC_FAIL_GEN;
	// set restitution
	poly_shape_def.restitution = 0.94f;
	poly_shape_def.friction    = 0.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.75f, 0.2f);
	// Add the ground shape to the ground body.
	wall7_->CreateShape(&poly_shape_def);
	wall7_->GetShapeList()->SetFilterData(filter);

	// create wall 8
	filter.categoryBits = 0x0004;
	filter.maskBits     = ~0x0002;
	body_def.position.Set(0.0f, 0.95f);
	wall8_ = pWorld->CreateBody(&body_def);
	if (!wall8_)
		return IC_FAIL_GEN;
	// set restitution
	poly_shape_def.restitution = 0.94f;
	poly_shape_def.friction    = 0.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.75f, 0.2f);
	// Add the ground shape to the ground body.
	wall8_->CreateShape(&poly_shape_def);
	wall8_->GetShapeList()->SetFilterData(filter);
	return IC_OK;
} // END FUNCTION InitPhysics(b2World* pWorld)