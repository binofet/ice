
#include "Render/icRenderObject.h"

#include "Render/Texture/icTexture.h"

/*! c'tor
**/
icRenderObject::icRenderObject(icGXDevice* pDevice)
{
    m_pDev = pDevice;
    m_pVB = NULL;
    m_pIB = NULL;
	m_pTex =NULL;

    m_m44Trans.Identity();
}// END FUNCTION icRenderObject(icGXDevice* pDevice)


/*! d'tor
**/
icRenderObject::~icRenderObject(void)
{
    Cleanup();
}// END FUNCTION ~icRenderObject(void)


/*! Releases allocated vertex buffers and such, overloadable
 *
 *  @returns        ICRESULT        Success/failure/warnings of cleanup action
**/
ICRESULT icRenderObject::Cleanup(void)
{
    ICRESULT res = IC_OK;
    if (m_pDev)
    {
        // release vertex buffer (if exists)
        if (m_pVB)
        {
            res |= m_pDev->ReleaseVB(m_pVB);
            m_pVB = NULL;
        }

        // release index buffer (if exists)
        if (m_pIB)
        {
            res |= m_pDev->ReleaseIB(m_pIB);
            m_pIB = NULL;
        }
    }    
    return res;
}// END FUNCTION Cleanup(void)


/*! Creates a renderable sphere
 *
 *  @param          radius          Radius of the sphere
 *  @param          col             Color of the sphere
 *  @param          subdiv          Subdivisions
**/
ICRESULT icRenderObject::CreateSphere(const icReal& radius, const icColor& color,
                                      int subdiv)
{
    // make sure we are starting fresh
    Cleanup();

    // 4 is the minimum number of subdivisions
    subdiv = icMax(4,subdiv);
    
    // there are two rings, the horizontal rings have half as many sub divisions
    // as the vertical rings.  note there are 2 extra verts for the top and bottom
    icVertDef vert_def;
    uint num_verts = (subdiv/2 - 1) * subdiv + 2;
    uint num_prims = 2 * (subdiv/2 - 2) * subdiv + 2 * subdiv;
    vert_def.numPrims = num_prims;
    vert_def.uiVertSize = sizeof(ICVRTNRMCOL);
    vert_def.uiNumVerts = num_verts;
    vert_def.usage = IC_VERT_USAGE_STATIC;
    vert_def.primType = IC_PRIM_TLIST;
    vert_def.vertType = IC_VERT_NRMCOL;

    icVertLock vertLock;

    // create and lock the vertex buffer
    if (ICEFAIL(m_pDev->CreateVB(&m_pVB, &vert_def)) ||
        ICEFAIL(m_pVB->Lock(&vertLock)))
    {
        Cleanup();
        return IC_FAIL_GEN;
    }

    // get a pointer to the locked verts
    ICVRTNRMCOL* vb = (ICVRTNRMCOL*)vertLock.pData;

    uint cur_vert=0;

    // top vert
    vb[cur_vert].pos.Set(0.0f,radius,0.0f);
    vb[cur_vert].col = color;
    vb[cur_vert++].norm.Set(0.0f,1.0f,0.0f);

    int vertical_subdiv = subdiv/2;

    if (subdiv < 6)
        vertical_subdiv = 2;

    // create the middle verticies
    icReal alpha,beta,x,y,z,r;
    for (int j=1; j<vertical_subdiv; ++j)
    {
        for (int i=0; i<subdiv; ++i)
        {
            alpha = (icReal)(2 * i * IC_PI)/(icReal)subdiv;

            beta = ((icReal)j/(icReal)vertical_subdiv) * IC_PI;

            if (subdiv > 5)
            {
                r = radius * icSin(beta);
                y = radius * icCos(beta);
            }
            else
            {
                y = 0.0f;
                r = radius;
            }

            x = r * icCos(alpha);
            z = r * icSin(alpha);

            vb[cur_vert].pos.Set(x,y,z);
            vb[cur_vert].col = color;

            // calc normal
            icVector3 normal = vb[cur_vert].pos;
            normal.Normalize();

            vb[cur_vert++].norm = normal;
        }
    }

    // bottom vert
    vb[cur_vert].pos.Set(0.0f,-radius,0.0f);
    vb[cur_vert].col = color;
    vb[cur_vert++].norm.Set(0.0f,1.0f,0.0f);

    // todo: check this result
    m_pVB->Unlock();
    
    icIndexDef ind_def;
    ind_def.indexSize = IC_INDEX_SIZE_16;
    ind_def.numIndex = num_prims*3;
    ind_def.usage = IC_INDEX_USAGE_STATIC;


    icIndexLock iblock;

    // make sure we can create and lock the index buffer
    if (ICEFAIL(m_pDev->CreateIB(&m_pIB,&ind_def)) ||
        ICEFAIL(m_pIB->Lock(&iblock)))
    {
        Cleanup();
        return IC_FAIL_GEN;
    }

    // get pointer to locked indicies
    short* ib = (short*)iblock.pData;
    uint cur_index = 0;

    // do top ring of faces
    for (int i=1; i<subdiv+1; ++i)
    {
        ib[cur_index++] = (short)i;
        ib[cur_index++] = 0;
        if (i < subdiv)
            ib[cur_index++] = (short)(i + 1);
        else
            ib[cur_index++] = 1;
    }

    // do middle ring faces (there is no middle ring for subdiv == 4 or 5)
    if (subdiv > 5)
    {
        for (int j=0; j<vertical_subdiv-2; ++j)
        {
            for (int i=1; i<subdiv+1; ++i)
            {
                ib[cur_index++] = (short)((subdiv * j) + i);
                if (i < subdiv)
                    ib[cur_index++] = (short)((subdiv * j) + i + 1);
                else
                    ib[cur_index++] = (short)((subdiv * j) + 1);
                ib[cur_index++] = (short)((subdiv * (j+1)) + i);


                ib[cur_index++] = (short)((subdiv * (j+1)) + i);
                if (i < subdiv)
                {
                    ib[cur_index++] = (short)((subdiv * j) + i + 1);
                    ib[cur_index++] = (short)((subdiv * (j+1)) + i + 1);
                }
                else
                {
                    ib[cur_index++] = (short)((subdiv * j) + 1);
                    ib[cur_index++] = (short)((subdiv * (j+1)) + 1);
                }
                
            }
        }
     }
    
    // do bottom ring faces
    for (int i=-1; i<subdiv-1; ++i)
    {
        ib[cur_index++] = (short)(num_verts - subdiv + i);
        ib[cur_index++] = (short)(num_verts-1);
        if (i < subdiv-2)
            ib[cur_index++] = (short)(num_verts - subdiv + i + 1);
        else
            ib[cur_index++] = (short)(num_verts - subdiv - 1);
    }

    // todo: check this result
    m_pIB->Unlock();

    return IC_OK;
}// END FUNCTION CreateSphere(const icReal& radius, const icColor& color,
 //                           int subdiv)


/*! Creates a renderable cube
 *
 *  @param          v3Dimensions    X,Y,Z dimensions of the box
 *  @param          color             Color of the box
 *  @param          subdiv          Subdivisions in each side
**/
ICRESULT icRenderObject::CreateBox(const icVector3& v3Dimensions, const icColor& color, int subdiv, icTexture* pTex)
{subdiv; // (warning removal) TODO: allow the cube to be subdivided
    // make sure we are starting fresh
    Cleanup();

    icVertDef vert_def;
    vert_def.numPrims = 12;
    if (pTex)
    {
        vert_def.uiVertSize = sizeof(ICVRTNRM_DIF);
        vert_def.vertType = IC_VERT_NRM_DIF;
        m_pTex = pTex;
    }
    else
    {
        vert_def.uiVertSize = sizeof(ICVRTNRMCOL);
        vert_def.vertType = IC_VERT_NRMCOL;
        m_pTex = NULL;
    }
    vert_def.uiNumVerts = 24;
    vert_def.usage = IC_VERT_USAGE_STATIC;
    vert_def.primType = IC_PRIM_TLIST;
    
    m_pDev->CreateVB(&m_pVB, &vert_def);

    icVertLock vertLock;
    m_pVB->Lock(&vertLock);

    ICVRTNRMCOL* vb = (ICVRTNRMCOL*)vertLock.pData;
    ICVRTNRM_DIF* vb_dif = (ICVRTNRM_DIF*)vb;

    icReal halfx = v3Dimensions.x/icReal(2);
    icReal halfy = v3Dimensions.y/icReal(2);
    icReal halfz = v3Dimensions.z/icReal(2);

    // Manually set vertices
    // Note: assume z extending toward you is positive, away negative
    // Face order: x, -x, y, -y, z, -z

    // Begin x face

    vb[0].pos.Set(halfx,-halfy,halfz);
    vb[0].norm.Set(1.0f,0.0f,0.0f);         
    
    vb[1].pos.Set(halfx,halfy,halfz);
    vb[1].norm.Set(1.0f,0.0f,0.0f);

    vb[2].pos.Set(halfx,halfy,-halfz);
    vb[2].norm.Set(1.0f,0.0f,0.0f);

    vb[3].pos.Set(halfx,-halfy,-halfz);
    vb[3].norm.Set(1.0f,0.0f,0.0f);

    if (pTex)
    {
        vb_dif[0].uv.Set(1.0f,0.0f);
        vb_dif[1].uv.Set(1.0f,1.0f);
        vb_dif[2].uv.Set(0.0f,1.0f);
        vb_dif[3].uv.Set(0.0f,0.0f);
    }
    else
    {
        vb[0].col = color;
        vb[1].col = color;
        vb[2].col = color;
        vb[3].col = color;
    }

    // End x face

    // Begin -x face

    vb[4].pos.Set(-halfx,-halfy,halfz);
    vb[4].norm.Set(1.0f,0.0f,0.0f);         
    
    vb[5].pos.Set(-halfx,halfy,halfz);
    vb[5].norm.Set(1.0f,0.0f,0.0f);

    vb[6].pos.Set(-halfx,halfy,-halfz);
    vb[6].norm.Set(1.0f,0.0f,0.0f);

    vb[7].pos.Set(-halfx,-halfy,-halfz);
    vb[7].norm.Set(1.0f,0.0f,0.0f);

    if (pTex)
    {
        vb_dif[4].uv.Set(0.0f,0.0f);
        vb_dif[5].uv.Set(0.0f,1.0f);
        vb_dif[6].uv.Set(1.0f,1.0f);
        vb_dif[7].uv.Set(1.0f,0.0f);
    }
    else
    {
        vb[4].col = color;
        vb[5].col = color;
        vb[6].col = color;
        vb[7].col = color;
    }

    // End -x face

    // Begin y face

    vb[8].pos.Set(-halfx,halfy,halfz);
    vb[8].norm.Set(0.0f,1.0f,0.0f);
    
    vb[9].pos.Set(-halfx,halfy,-halfz);
    vb[9].norm.Set(0.0f,1.0f,0.0f);

    vb[10].pos.Set(halfx,halfy,-halfz);
    vb[10].norm.Set(0.0f,1.0f,0.0f);

    vb[11].pos.Set(halfx,halfy,halfz);
    vb[11].norm.Set(0.0f,1.0f,0.0f);

    if (pTex)
    {
        vb_dif[8].uv.Set(0.0f,1.0f);
        vb_dif[9].uv.Set(0.0f,0.0f);
        vb_dif[10].uv.Set(1.0f,0.0f);
        vb_dif[11].uv.Set(1.0f,1.0f);
    }
    else
    {
        vb[8].col = color;
        vb[9].col = color;
        vb[10].col = color;
        vb[11].col = color;
    }
    // End y face

    // Begin -y face

    vb[12].pos.Set(-halfx,-halfy,halfz);
    vb[12].norm.Set(0.0f,-1.0f,0.0f);
    
    vb[13].pos.Set(-halfx,-halfy,-halfz);
    vb[13].norm.Set(0.0f,-1.0f,0.0f);

    vb[14].pos.Set(halfx,-halfy,-halfz);
    vb[14].norm.Set(0.0f,-1.0f,0.0f);

    vb[15].pos.Set(halfx,-halfy,halfz);
    vb[15].norm.Set(0.0f,-1.0f,0.0f);

    if (pTex)
    {
        vb_dif[12].uv.Set(0.0f,0.0f);
        vb_dif[13].uv.Set(0.0f,1.0f);
        vb_dif[14].uv.Set(1.0f,1.0f);
        vb_dif[15].uv.Set(1.0f,0.0f);
    }
    else
    {
        vb[12].col = color;
        vb[13].col = color;
        vb[14].col = color;
        vb[15].col = color;
    }
    // End -y face

    // Begin z face

    vb[16].pos.Set(-halfx,-halfy,halfz);
    vb[16].norm.Set(0.0f,0.0f,1.0f);            
    
    vb[17].pos.Set(-halfx,halfy,halfz);
    vb[17].norm.Set(0.0f,0.0f,1.0f);            

    vb[18].pos.Set(halfx,halfy,halfz);
    vb[18].norm.Set(0.0f,0.0f,1.0f);            

    vb[19].pos.Set(halfx,-halfy,halfz);
    vb[19].norm.Set(0.0f,0.0f,1.0f);            

    if (pTex)
    {
        vb_dif[16].uv.Set(1.0f,0.0f);
        vb_dif[17].uv.Set(1.0f,1.0f);
        vb_dif[18].uv.Set(0.0f,1.0f);
        vb_dif[19].uv.Set(0.0f,0.0f);
    }
    else
    {
        vb[16].col = color;
        vb[17].col = color;
        vb[18].col = color;
        vb[19].col = color;
    }
    // End z face

    // Begin -z face

    vb[20].pos.Set(-halfx,-halfy,-halfz);
    vb[20].norm.Set(0.0f,0.0f,-1.0f);            
    
    vb[21].pos.Set(-halfx,halfy,-halfz);
    vb[21].norm.Set(0.0f,0.0f,-1.0f);            

    vb[22].pos.Set(halfx,halfy,-halfz);
    vb[22].norm.Set(0.0f,0.0f,-1.0f);            

    vb[23].pos.Set(halfx,-halfy,-halfz);
    vb[23].norm.Set(0.0f,0.0f,-1.0f);            

    if (pTex)
    {
        vb_dif[20].uv.Set(0.0f,0.0f);
        vb_dif[21].uv.Set(0.0f,1.0f);
        vb_dif[22].uv.Set(1.0f,1.0f);
        vb_dif[23].uv.Set(1.0f,0.0f);
    }
    else
    {
        vb[20].col = color;
        vb[21].col = color;
        vb[22].col = color;
        vb[23].col = color;
    }
    // End -z face

    m_pVB->Unlock();
    
    icIndexDef ind_def;
    ind_def.indexSize = IC_INDEX_SIZE_16;
    ind_def.numIndex = 6*2*3;
    ind_def.usage = IC_INDEX_USAGE_STATIC;
    m_pDev->CreateIB(&m_pIB,&ind_def);

    icIndexLock iblock;
    m_pIB->Lock(&iblock);

    short* ib = (short*)iblock.pData;
    uint cur_index = 0;

    // Create index buffer for each face (4 vertices per face)
    for(uint vert_index=0; vert_index<vert_def.uiNumVerts; vert_index+=4)
    {
        // First triangle on face
        ib[cur_index++]=(short)vert_index; ib[cur_index++]=(short)(vert_index+1); ib[cur_index++]=(short)(vert_index+2);
        // Second triangle on face
        ib[cur_index++]=(short)vert_index; ib[cur_index++]=(short)(vert_index+2); ib[cur_index++]=(short)(vert_index+3);
    }

    m_pIB->Unlock();

    return IC_OK;
}// END FUNCTION CreateBox(const icVector3& v3Dimensions, const icColor& color,
 //                        int subdiv)


/*! Creates a renderable square plane
 *
 *  @param          width           Length of either side
 *  @param          col             Color of the plane
 *  @param          subdiv          Subdivisions in plane
**/
ICRESULT icRenderObject::CreatePlane(const icReal& length, const icReal& width,
                         const icColor& color, int numrow, int numcol)
{
    // make sure we are starting fresh
    Cleanup();

    icVertDef vert_def;
    vert_def.numPrims = (numrow-1)*(numcol-1)*2;
    vert_def.uiVertSize = sizeof(ICVRTNRMCOL);
    vert_def.uiNumVerts = (numrow-1)*(numcol-1)*4;
    vert_def.usage = IC_VERT_USAGE_STATIC;
    vert_def.primType = IC_PRIM_TLIST;
    vert_def.vertType = IC_VERT_NRMCOL;
    m_pDev->CreateVB(&m_pVB, &vert_def);

    icVertLock vertLock;
    m_pVB->Lock(&vertLock);

    ICVRTNRMCOL* vb = (ICVRTNRMCOL*)vertLock.pData;

    uint cur_vert=0;
    icReal halfx = width/icReal(2);
    icReal halfy = length/icReal(2);
    icReal edgeDistx = width/icReal(numrow-1);
    icReal edgeDisty = length/icReal(numcol-1);

    // TODO: this currently has extra verts to allow for tiled texturing
    //       need to add the texture option and make this better
    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            icReal x1 = (col*edgeDistx)-halfx;
            icReal y1 = (row*edgeDisty)-halfy;
            icReal x2 = ((col+1)*edgeDistx)-halfx;
            icReal y2 = ((row+1)*edgeDisty)-halfy;

            vb[cur_vert].pos.Set(x1,0.0f,y1);
            vb[cur_vert].col = color;
            vb[cur_vert++].norm.Set(0.0f,1.0f,0.0f);

            vb[cur_vert].pos.Set(x1,0.0f,y2);
            vb[cur_vert].col = color;
            vb[cur_vert++].norm.Set(0.0f,1.0f,0.0f);

            vb[cur_vert].pos.Set(x2,0.0f,y2);
            vb[cur_vert].col = color;
            vb[cur_vert++].norm.Set(0.0f,1.0f,0.0f);            

            vb[cur_vert].pos.Set(x2,0.0f,y1);
            vb[cur_vert].col = color;
            vb[cur_vert++].norm.Set(0.0f,1.0f,0.0f);
        }
    }

    m_pVB->Unlock();
    
    icIndexDef ind_def;
    ind_def.indexSize = IC_INDEX_SIZE_16;
    ind_def.numIndex = (numrow-1)*(numcol-1)*6;
    ind_def.usage = IC_INDEX_USAGE_STATIC;
    m_pDev->CreateIB(&m_pIB,&ind_def);

    icIndexLock iblock;
    m_pIB->Lock(&iblock);

    short* ib = (short*)iblock.pData;
    uint cur_index = 0;


    for (int row=0; row<numrow-1; ++row)
    {
        for (int col=0; col<numcol-1; ++col)
        {
            ib[cur_index++] = (short)((numcol-1)*row*4 + col*4);
            ib[cur_index++] = (short)((numcol-1)*row*4 + col*4 + 1);
            ib[cur_index++] = (short)((numcol-1)*row*4 + col*4 + 2);

            ib[cur_index++] = (short)((numcol-1)*row*4 + col*4 + 2);
            ib[cur_index++] = (short)((numcol-1)*row*4 + col*4 + 3);
            ib[cur_index++] = (short)((numcol-1)*row*4 + col*4);
        }
    }
    m_pIB->Unlock();
    return IC_OK;
}// END FUNCTION CreatePlane(const icReal& width, const icColor& color,
 //                          int subdiv)


/*! Creates a renderable line strip
 *
 *  @param          pVerts          Array of verts
 *  @param          color           Color of the lines
 *  @param          numPoints       Number of points in pVerts
**/
ICRESULT icRenderObject::CreateLines(icVector3* pVerts, const icColor& color,
					 				 int numPoints, bool list)
{
    // make sure we are starting fresh
    Cleanup();

    if (m_pDev && numPoints > 1)
    {
        icVertDef vert_def;
        
        // line strip or list?
        if (!list)
        {
            vert_def.numPrims = numPoints-1;
            vert_def.primType = IC_PRIM_LSTRIP;
        }
        else
        {
            vert_def.numPrims = numPoints/2;
            vert_def.primType = IC_PRIM_LLIST;
        }
        vert_def.uiNumVerts = numPoints;
        vert_def.uiVertSize = sizeof(ICVRTCOL);
        vert_def.usage = IC_VERT_USAGE_DYNAMIC;
        vert_def.vertType = IC_VERT_COL;

        // create vertex buffer on the rendering device
        if (ICEOK(m_pDev->CreateVB(&m_pVB,&vert_def)))
        {
            icVertLock vert_lock;

            // lock the buffer to fill it with our data
            if (ICEOK(m_pVB->Lock(&vert_lock)))
            {
                // get pointer to locked data
                ICVRTCOL* pvb = (ICVRTCOL*)vert_lock.pData;

                // fill verts
                for (int i=0; i<numPoints; ++i)
                {
                    pvb[i].pos = pVerts[i];
                    pvb[i].col = color;
                }

                // unlock data, should check this result too
                m_pVB->Unlock();

                return IC_OK;
            }
            return IC_FAIL_LOCK_VB;
        }
        return IC_FAIL_CREATE_VB;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateLines(icVector3* pVerts, const icColor& color,
 //	                         int numPoints, bool list)


/*! Overloadable update function
 *
 *  @param          fDeltaTime      Elapsed time in seconds since last update
 *  @returns        ICRESULT        Success/failure/warnings of render call
**/
ICRESULT icRenderObject::Update(const float fDeltaTime)
{
    fDeltaTime; // warning removal
    return IC_OK;
}// END FUNCTION Update(const float fDeltaTime)


/*! Overloadable render function
 *
 *  @returns        ICRESULT        Success/failure/warnings of render call
**/
ICRESULT icRenderObject::Render(void)
{
    if (m_pTex)
        m_pDev->SetTexture(0,m_pTex);

    m_pDev->SetWorldTransform(m_m44Trans);
    if (m_pIB)	
        return m_pDev->DrawIndexedVertBuf(m_pVB, m_pIB);
    	
    return m_pDev->DrawVertBuf(m_pVB);

}// END FUNCTION Render(void)
