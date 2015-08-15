
#include "skeeball_ramp.h"

#include "Content/icContentLoader.h"
#include "Core/IO/icFile.h"

#include "Render/icModel.h"

#include "Render/VertBuffer/icVertBuffer.h"

#include "btBulletDynamicsCommon.h"


/*! c'tor
**/
Ramp::Ramp(void)
{
    m_pTable = NULL;

    m_verts = NULL;
    m_ib = NULL;

    m_vb = NULL;
}// END FUNCTION Ramp(void)


/*! d'tor
**/
Ramp::~Ramp(void)
{
    if (m_verts)
        delete[] m_verts;

    if (m_ib)
        delete[] m_ib;

    if (m_vb)
        free(m_vb);
}// END FUNCTION ~Ramp(void)


/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
**/
ICRESULT Ramp::Init(icContentLoader* pContent, btDiscreteDynamicsWorld* pWorld)
{
    ICRESULT res = IC_OK;
    if (!pContent)
        return IC_FAIL_GEN;

    m_pContent = pContent;

    // load the Ramp model
    res |= m_pContent->Load("Resource/models/skeeball_table.icm",&m_pTable);

    m_Trans.Identity();
    m_Trans.RotX(-IC_PI/2.0f);

    res |= InitPhysics(pWorld);

    return res;
}// END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)


/*! Updates the Ramp
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
**/
ICRESULT Ramp::Update(const float fDeltaTime)
{
    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)

//static icModel* model;

/*! Renders the Ramp
 *
 *  @returns        ICRESULT        Success/failure of render
**/
ICRESULT Ramp::Render(void)
{
    ICRESULT res = IC_OK;

    icMatrix44 temp_mat;

    res = m_pContent->GetDevice()->SetAmbientColor(icColor::BRED);

    if (m_pworld)
    {
        m_colObject->getWorldTransform().getOpenGLMatrix((btScalar*)&temp_mat);
        temp_mat.Scale(-1.0f,1.0f,1.0f);
        res |= m_pTable->Render(temp_mat);

        m_colObject2->getWorldTransform().getOpenGLMatrix((btScalar*)&temp_mat);
        temp_mat.Scale(-1.0f,1.0f,1.0f);
        res |= m_pTable->Render(temp_mat);

        m_colObject3->getWorldTransform().getOpenGLMatrix((btScalar*)&temp_mat);
        temp_mat.Scale(-1.0f,1.0f,1.0f);
        res |= m_pTable->Render(temp_mat);
    }
    else
    {
        temp_mat.Identity();
        temp_mat.RotX(-IC_PI_HALF);
        temp_mat.SetPos(-105.422f,0.0f,0.0f);
        temp_mat.Scale(-1.0f,1.0f,1.0f);
        m_pTable->Render(temp_mat);
        temp_mat.Scale(-1.0f,1.0f,1.0f);

        temp_mat.Scale(-1.0f,1.0f,1.0f);
        temp_mat.SetPos(0.0f,0.0f,0.0f);
        m_pTable->Render(temp_mat);
        temp_mat.Scale(-1.0f,1.0f,1.0f);

        temp_mat.Scale(-1.0f,1.0f,1.0f);
        temp_mat.SetPos(105.422f,0.0f,0.0f);
        m_pTable->Render(temp_mat);
    }

    return res;
}// END FUNCTION Render(void)



/*! Initializes the physics for the Ramp
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
**/
ICRESULT Ramp::InitPhysics(btDiscreteDynamicsWorld* pWorld)
{
    if (!pWorld) return IC_OK;;


    m_pworld = pWorld;

    if (m_verts)
        delete[] m_verts;

    if (m_ib) 
        delete[] m_ib;


    
    //m_pContent->Load("Resource/models/skeeball_collision.icm",&model);

    //// try to open the model file
    icFile file;
    if (ICEFAIL(file.Open("Resource/models/skeeball_collision.icm", ICFMREAD_EXISTING)))
    {
        icWarningf("Content loader could not open: %s", "Resource/models/skeeball_collision.icm");
        return IC_FAIL_GEN;
    }

    _ICE_MODEL model_header = {0};

    size_t sizeread = 0;

    // read the ice model header
    if (ICEFAIL(file.Read(&model_header, sizeof(_ICE_MODEL), &sizeread)))
    {
        icWarningf("Content loader could not read: %s", "Resource/models/skeeball_collision.icm");
        return IC_FAIL_GEN;
    }

    
    m_verts = NULL;

    int totalTriangles = model_header.numInd/3;

    int totalVerts = model_header.numVerts;

    m_verts = new icVector3[model_header.numVerts];

    m_ib = new int [model_header.numInd];

    m_vb = malloc(sizeof(ICVRTNRM_DIF)*model_header.numVerts);

    size_t read;
    // CREATE VERTEX BUFFER
    switch(model_header.modelVersion)
    {
    case ICE_MODEL_FORMAT:
        {
            switch (model_header.vertType)
            {
            case IC_VERT_DIF:
                {
                    ICVRT_DIF cur_vert;
                    for (int i=0; i<model_header.numVerts; ++i)
                    {
                        file.Read(&cur_vert,sizeof(ICVRT_DIF),&read);
                        m_verts[i] = cur_vert.pos;
                    }
                }break;
            case IC_VERT_NRM_DIF:
                {
                    ICVRTNRM_DIF cur_vert;
                    for (int i=0; i<model_header.numVerts; ++i)
                    {
                        file.Read(&cur_vert,sizeof(ICVRTNRM_DIF),&read);
                        m_verts[i] = cur_vert.pos;
                    }
                }break;
            default:
                ;
            }
        }break;
    case 101:
        {
            switch (model_header.vertType)
            {
            case 2:
                {
                    ICVRT_DIF cur_vert;
                    for (int i=0; i<model_header.numVerts; ++i)
                    {
                        file.Read(&cur_vert,sizeof(ICVRT_DIF),&read);
                        m_verts[i] = cur_vert.pos;
                    }
                }break;
            case 8:
                {
                    ICVRTNRM_DIF cur_vert;
                    for (int i=0; i<model_header.numVerts; ++i)
                    {
                        file.Read(&cur_vert,sizeof(ICVRT_DIF),&read);
                        m_verts[i] = cur_vert.pos;
                    }
                }break;
            default:
                {
                    ICVRTNRM_DIF cur_vert;
                    for (int i=0; i<model_header.numVerts; ++i)
                    {
                        file.Read(&cur_vert,sizeof(ICVRT_DIF),&read);
                        m_verts[i] = cur_vert.pos;
                    }
                }
            }
        }break;
    case 100:
        {
            file.SetPos(sizeof(_ICE_MODEL_100)); // rewind

            ICVRT_DIF cur_vert;
            for (int i=0; i<model_header.numVerts; ++i)
            {
                file.Read(&cur_vert,sizeof(ICVRT_DIF),&read);
                m_verts[i] = cur_vert.pos;
            }
        }break;
    default:
        icWarningf("Invalid model format: %s", "Resource/models/skeeball_collision.icm");
        return IC_FAIL_GEN;
    }

    file.Read(m_ib,model_header.numInd*sizeof(int),&read);

    m_indexVertexArrays = new btTriangleIndexVertexArray(totalTriangles,
        m_ib,
        3*sizeof(int),
        model_header.numVerts,(btScalar*)m_verts,sizeof(icVector3));
    
    btVector3 aabbMin(-1000,-1000,-1000),aabbMax(1000,1000,1000);
    m_colShape = new btBvhTriangleMeshShape(m_indexVertexArrays,true,aabbMin,aabbMax);


    m_colObject = new btCollisionObject();
    m_colObject->setCollisionShape(m_colShape);

    m_colObject2 = new btCollisionObject();
    m_colObject2->setCollisionShape(m_colShape);

    m_colObject3 = new btCollisionObject();
    m_colObject3->setCollisionShape(m_colShape);

	btTransform transform;
    transform.setFromOpenGLMatrix((btScalar*)m_Trans);
    m_colObject->setWorldTransform(transform);

    transform.setOrigin(btVector3(-105.422f,0.0f,0.0f));
    m_colObject2->setWorldTransform(transform);

    transform.setOrigin(btVector3(105.422f,0.0f,0.0f));
    m_colObject3->setWorldTransform(transform);



    pWorld->addCollisionObject(m_colObject);//, 0x0001, 0x00FF);
    pWorld->addCollisionObject(m_colObject2);//, 0x0001, 0x00FF);
    pWorld->addCollisionObject(m_colObject3);//, 0x0001, 0x00FF);

    return IC_OK;
}// END FUNCTION InitPhysics(b2World* pWorld)