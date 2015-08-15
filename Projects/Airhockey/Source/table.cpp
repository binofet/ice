
#include "table.h"

#include "Content/icContentLoader.h"

#include "../Box2D/Box2D.h"


/*! c'tor
**/
Table::Table(void)
{
    m_pTable = NULL;
    m_pTable2 = NULL;
    m_pTable3 = NULL;
    m_pTable4 = NULL;

    m_pWall1 = NULL;
    m_pWall2 = NULL;
    m_pWall3 = NULL;
    m_pWall4 = NULL;
    m_pWall5 = NULL;
    m_pWall6 = NULL;
    m_pWall7 = NULL;
    m_pWall8 = NULL;
}// END FUNCTION Table(void)


/*! d'tor
**/
Table::~Table(void)
{
}// END FUNCTION ~Table(void)


/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT Table::Init(icContentLoader* pContent, b2World* pWorld)
{
    ICRESULT res = IC_OK;
    if (!pContent)
        return IC_FAIL_GEN;

    m_pContent = pContent;

    // load the table model
    res |= m_pContent->Load("Resource/models/table.icm",&m_pTable);
    res |= m_pContent->Load("Resource/models/table_2.icm",&m_pTable2);
    //res |= m_pContent->Load("Resource/models/table_3.icm",&m_pTable3);
    //res |= m_pContent->Load("Resource/models/table_4.icm",&m_pTable4);

    m_Trans.Identity();
    m_Trans.RotY(IC_PI);
    m_Trans.RotX(-IC_PI/2.0f);

    res |= InitPhysics(pWorld);

    return res;
}// END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)


/*! Updates the table
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
**/
ICRESULT Table::Update(const float fDeltaTime)
{
    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)


/*! Renders the table
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT Table::Render(void)
{
    ICRESULT res = IC_OK;
    res = m_pTable->Render(m_Trans);
    res |= m_pTable2->Render(m_Trans);
    //m_pTable3->Render(m_Trans);
    //m_pTable4->Render(m_Trans);
    return res;
}// END FUNCTION Render(void)


/*! Initializes the physics for the table
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT Table::InitPhysics(b2World* pWorld)
{
    if (!pWorld) return IC_FAIL_GEN;

    // get some of these
	b2BodyDef body_def;
    b2PolygonDef poly_shape_def;

    // create wall 1
	body_def.position.Set(-1.15f/3.0f, -0.835f);
    m_pWall1 = pWorld->CreateBody(&body_def);
    if (!m_pWall1) return IC_FAIL_GEN;
    // set restitution
    poly_shape_def.restitution = 0.30f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	m_pWall1->CreateShape(&poly_shape_def);

    // create wall 2
	body_def.position.Set(-1.15f/3.0f, 0.95f);
    m_pWall2 = pWorld->CreateBody(&body_def);
    if (!m_pWall2) return IC_FAIL_GEN;
    //poly_shape_def.restitution = 1.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	m_pWall2->CreateShape(&poly_shape_def);

    // create wall 3
	body_def.position.Set(-0.55f, 0.0f);
	m_pWall3 = pWorld->CreateBody(&body_def);
    if (!m_pWall3) return IC_FAIL_GEN;
    //poly_shape_def.restitution = 1.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.2f, 2.025f);
	// Add the ground shape to the ground body.
	m_pWall3->CreateShape(&poly_shape_def);


    // create wall 4
	body_def.position.Set(1.0f/3.0f, 0.95f);
    m_pWall4 = pWorld->CreateBody(&body_def);
    if (!m_pWall4) return IC_FAIL_GEN;
    //poly_shape_def.restitution = 1.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	m_pWall4->CreateShape(&poly_shape_def);


    // create wall 5
	body_def.position.Set(0.55f, 0.0f);
	m_pWall5 = pWorld->CreateBody(&body_def);
    if (!m_pWall5) return IC_FAIL_GEN;
    //poly_shape_def.restitution = 1.0f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.2f, 2.025f);
	// Add the ground shape to the ground body.
	m_pWall5->CreateShape(&poly_shape_def);

    // create wall 6
	body_def.position.Set(1.0f/3.0f, -0.835f);
    m_pWall6 = pWorld->CreateBody(&body_def);
    if (!m_pWall6) return IC_FAIL_GEN;
    // set restitution
    poly_shape_def.restitution = 0.30f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.175f, 0.2f);
	// Add the ground shape to the ground body.
	m_pWall6->CreateShape(&poly_shape_def);

    // create wall 7
    b2FilterData filter;
    filter.categoryBits = 0x0001;
    filter.maskBits = ~0x0002;
	body_def.position.Set(0.0f, -0.835f);
    m_pWall7 = pWorld->CreateBody(&body_def);
    if (!m_pWall7) return IC_FAIL_GEN;
    // set restitution
    poly_shape_def.restitution = 0.30f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.75f, 0.2f);
	// Add the ground shape to the ground body.
	m_pWall7->CreateShape(&poly_shape_def);
    m_pWall7->GetShapeList()->SetFilterData(filter);

    // create wall 8
    filter.categoryBits = 0x0004;
    filter.maskBits = ~0x0002;
	body_def.position.Set(0.0f, 0.95f);
    m_pWall8 = pWorld->CreateBody(&body_def);
    if (!m_pWall8) return IC_FAIL_GEN;
    // set restitution
    poly_shape_def.restitution = 0.30f;
	// The extents are the half-widths of the box.
	poly_shape_def.SetAsBox(0.75f, 0.2f);
	// Add the ground shape to the ground body.
	m_pWall8->CreateShape(&poly_shape_def);
    m_pWall8->GetShapeList()->SetFilterData(filter);
    return IC_OK;
}// END FUNCTION InitPhysics(b2World* pWorld)