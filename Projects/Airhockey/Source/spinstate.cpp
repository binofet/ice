//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "spinstate.h"


#include "Core/IO/icInput.h"

#include "Math/icRand.h"


extern icVector2 bl;
extern icVector2 tl;
extern icVector2 tr;
extern icVector2 br;


SpinState::SpinState()
{
    m_pTable = NULL;
    m_pTable2 = NULL;
    m_pTable3 = NULL;
    m_pTable4 = NULL;
    m_pContent = NULL;
    m_pGroundTex = NULL;
    m_pGround = NULL;

    halfx = 500.0;
    halfy = 500.0;
    numrow = 50;
    numcol = 50;
    edgeDist = (halfx*2)/(numrow-1);

    cam_pos.Set(0.0f,0.750f,-1.54f);
    look_pos.Set(0.0f,0.0f,0.0f);

    m_bRenderTopOnly = false;
    m_bUpdateTopOnly = false;
}

SpinState::~SpinState()
{
}


/*! Initializes the games state
 *
 *  @param          pContentLoader      Pointer to main content loader
 *  @returns        ICRESULT            Success/failure/warnings of init
**/
ICRESULT SpinState::Init(icContentLoader* pContentLoader)
{
    if (!pContentLoader)
        return IC_FAIL_GEN;

    m_pContent = pContentLoader;

    icGXDevice* pDev = m_pContent->GetDevice();

    // CREATE A TEMPORARY GROUND OBJECT

    icVertDef vert_def;
    vert_def.numPrims = (numrow-1)*(numcol-1)*2;
    vert_def.uiVertSize = sizeof(ICVRT_DIF);
    vert_def.uiNumVerts = (numrow-1)*(numcol-1)*4;
    vert_def.usage = IC_VERT_USAGE_STATIC;
    vert_def.primType = IC_PRIM_TLIST;
    vert_def.vertType = IC_VERT_DIF;
    pDev->CreateVB(&m_pGround, &vert_def);

    icVertLock vertLock;
    m_pGround->Lock(&vertLock);

    ICVRT_DIF* vb = (ICVRT_DIF*)vertLock.pData;

    uint cur_vert=0;

    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            icReal x1 = (col*edgeDist)-halfx;
            icReal y1 = (row*edgeDist)-halfy;
            icReal x2 = ((col+1)*edgeDist)-halfx;
            icReal y2 = ((row+1)*edgeDist)-halfy;

            vb[cur_vert].pos.Set(x1,-10.0f,y1);
            vb[cur_vert++].uv.Set(0.0f,0.0f);

            vb[cur_vert].pos.Set(x1,-10.0f,y2);
            vb[cur_vert++].uv.Set(0.0f,1.0f);

            vb[cur_vert].pos.Set(x2,-10.0f,y2);
            vb[cur_vert++].uv.Set(1.0f,1.0f);

            vb[cur_vert].pos.Set(x2,-10.0f,y1);
            vb[cur_vert++].uv.Set(1.0f,0.0f);
        }
    }

    m_pGround->Unlock();
    m_pContent->Load("Resource/textures/fire.tga", &m_pGroundTex);


    icIndexDef ind_def;
    ind_def.indexSize = IC_INDEX_SIZE_16;
    ind_def.numIndex = (numrow-1)*(numcol-1)*6;
    ind_def.usage = IC_INDEX_USAGE_STATIC;
    pDev->CreateIB(&m_pGroundIB,&ind_def);

    icIndexLock iblock;
    m_pGroundIB->Lock(&iblock);

    short* ib = (short*)iblock.pData;
    uint cur_index = 0;


    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            ib[cur_index++] = (numcol-1)*row*4 + col*4 ;
            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 1;
            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 2;

            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 2;
            ib[cur_index++] = (numcol-1)*row*4 + col*4 + 3;
            ib[cur_index++] = (numcol-1)*row*4 + col*4;
        }
    }
    m_pGroundIB->Unlock();

    ICRESULT res = IC_OK;

    // load the table model
    res |= m_pContent->Load("Resource/models/table.icm",&m_pTable);
    res |= m_pContent->Load("Resource/models/table_2.icm",&m_pTable2);
    //res |= m_pContent->Load("Resource/models/table_3.icm",&m_pTable3);
    //res |= m_pContent->Load("Resource/models/table_4.icm",&m_pTable4);

    m_Transform.Identity();

    // get us a camera
    icCreatePerspective((40.0f*IC_PI/180.0f),1280.0f/720.0f,1.0f,10000000.0f, m_Camera.GetProjection());

    org_len = cam_pos.Length();
    icCreateLookAt(cam_pos,look_pos,icVector3(0.0f,1.0f,0.0f),m_Camera.GetViewMat()); 

    return res;
}// END FUNCTION Init(icContentLoader* pContentLoader)


/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
**/
ICRESULT SpinState::Update(const float fDeltaTime, bool *bFinished)
{
    icInput* pInput = icInput::Instance();
    short x,y;
    pInput->GetPos(&x,&y);
    icVector2 curPos((icReal)x,(icReal)y);

    // camera movement
    icMatrix44& view = m_Camera.GetViewMat();
    icVector3& pos = view.GetPos();

    static float x_rot = 0.0f;
    static float y_rot = 0.0f;

    icVector3 new_pos = cam_pos;

    x_rot += 0.2f * fDeltaTime;

    while (x_rot >= 2*IC_PI)
        x_rot -= 2*IC_PI;

    icMatrix44 temp;
    temp.Identity();
    temp.RotY(x_rot);
    new_pos = temp.TransformVect(new_pos);
    float cur_len = cam_pos.Length();
    icCreateLookAt(new_pos,look_pos,icVector3(0.0f,1.0f,0.0f),m_Camera.GetViewMat());

    icVertLock vlock;
    m_pGround->Lock(&vlock);

    ICVRT_DIF* vb = (ICVRT_DIF*)vlock.pData;

    static float curdir = 0.0f;

    icReal u1 = icCos(curdir) * 0.01f * fDeltaTime;
    icReal v2 = icSin(curdir) * 0.01f * fDeltaTime;

    icReal t = icRandF(0.0001f,0.001f);
    curdir += icRandF(0.0f,10000000.0f) > 500000.0f ? t : -t;

    uint cur_vert = 0;
    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
            vb[cur_vert].uv.x += u1;
            vb[cur_vert++].uv.y += v2;
        }
    }

    m_pGround->Unlock();

    // TODO: if score, win state and such
    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime, bool *bFinished)


/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
**/
ICRESULT SpinState::Render(void)
{
    icGXDevice* pDev = m_pContent->GetDevice();

    // set projection and view matrix
    pDev->SetProjection(m_Camera.GetProjection());
    pDev->SetViewMatrix(m_Camera.GetViewMat());

    //pDev->SetCullMode(IC_CULL_OFF);
    // draw the temporary ground
    m_Transform.Identity();
    pDev->SetWorldTransform(m_Transform);
    pDev->SetTexture(0, m_pGroundTex);
    pDev->DrawIndexedVertBuf(m_pGround,m_pGroundIB);

    // draw the table
    m_Transform.RotY(IC_PI);
    m_Transform.RotX(-IC_PI/2.0f);
    m_pTable->Render(m_Transform);
    m_pTable2->Render(m_Transform);
    //m_pTable3->Render(m_Transform);
    //m_pTable4->Render(m_Transform);
    m_Transform.Identity();

    return IC_OK;
}// END FUNCTION Render(void)


/*! This gets called when the SpinState gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
**/
void SpinState::OnPush(void)
{
}// END FUNCTION OnPush(void)


/*! This gets called when the SpinState gets popped off the stack
 *  we might not need it
**/
void SpinState::OnPop(void)
{
}// END FUNCTION OnPop(void)