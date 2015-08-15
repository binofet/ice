#include "Content/icContentLoader.h"

#include "Core/IO/icFile.h"

#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Texture/icTexture.h"
#include "Render/icModel.h"

/*! Model loader
 *
 *  @param          szFileName      Name of the model to load
 *  @param[in/out]  ppObj           Storage place for loaded texture
 *  @returns        ICRESULT        Success/failure of texture load
**/
template<>
ICRESULT icContentLoader::Load<icModel>(const char* szFileName,
                                        icModel** ppObj)
{
    if (!szFileName || !strlen(szFileName))
    {
        *ppObj = NULL;
        return IC_WARN_GEN;
    }

    // try to open the model file
    icFile file;
    if (ICEFAIL(file.Open(szFileName, ICFMREAD_EXISTING)))
    {
        icWarningf("Content loader could not open: %s", szFileName);
        return IC_FAIL_GEN;
    }

    _ICE_MODEL model_header = {0};

    size_t sizeread = 0;

    // read the ice eader
    if (ICEFAIL(file.Read(&model_header, sizeof(_ICE_MODEL), &sizeread)))
    {
        icWarningf("Content loader could not read: %s", szFileName);
        return IC_FAIL_GEN;
    }

    // Get new model and set its device pointer
    icModel* model = new icModel();
    model->m_pDevice = m_pDevice;

    // CREATE VERTEX BUFFER
    icVertDef vert_def = {0};
    vert_def.uiNumVerts = model_header.numVerts;
    switch(model_header.modelVersion)
    {
    case ICE_MODEL_FORMAT:
        vert_def.vertType = (IC_VERT)model_header.vertType;
        switch (vert_def.vertType)
        {
        case IC_VERT_DIF:
            vert_def.uiVertSize = sizeof(ICVRT_DIF);
            break;
        case IC_VERT_NRM_DIF:
            vert_def.uiVertSize = sizeof(ICVRTNRM_DIF);
            break;
        default: vert_def.uiVertSize = 0;
        }
        break;
    case 101:
        switch (model_header.vertType)
        {
        case 2:
            vert_def.vertType = IC_VERT_DIF;
            vert_def.uiVertSize = sizeof(ICVRT_DIF);
            break;
        case 8:
            vert_def.vertType = IC_VERT_NRM_DIF;
            vert_def.uiVertSize = sizeof(ICVRTNRM_DIF);
            break;
        default: vert_def.uiVertSize = 0;
        }
        break;
    case 100:
        vert_def.vertType = IC_VERT_DIF;
        vert_def.uiVertSize = sizeof(ICVRT_DIF);
        file.SetPos(sizeof(_ICE_MODEL_100)); // rewind
        break;
    default:
        delete model;
        icWarningf("Invalid model format: %s", szFileName);
        return IC_FAIL_GEN;
    }

    vert_def.usage = IC_VERT_USAGE_STATIC;
    vert_def.numPrims = model_header.numInd / 3;
    vert_def.primType = IC_PRIM_TLIST;

    icVertBuffer* vb;
    if (ICEFAIL(m_pDevice->CreateVB(&vb, &vert_def)))
    {
        delete model;
        icWarningf("Content loader could not create models (%s) vertex buffer", model_header.modelName);
        return IC_FAIL_GEN;
    }
    // fill the vertex buffer
    icVertLock vertLock;
    if (ICEFAIL(vb->Lock(&vertLock)))
    {
        m_pDevice->ReleaseVB(vb);
        delete model;
        icWarningf("Content loader count not lock models (%s) vert buffer",
                   model_header.modelName);
        return IC_FAIL_GEN;
    }
    if (ICEFAIL(file.Read(vertLock.pData, vertLock.uiNumVerts*vertLock.uiVertSize, &sizeread)))
    {
        m_pDevice->ReleaseVB(vb);
        delete model;
        icWarningf("Content loader count not read models (%s) vert buffer",
                   model_header.modelName);
        return IC_FAIL_GEN;
    }
    vb->Unlock();
    model->m_pVB = vb;

    // CREATE INDEX BUFFER
    icIndexDef index_def = {0};
    index_def.numIndex = model_header.numInd;
    index_def.indexSize = IC_INDEX_SIZE_32;
    index_def.usage = IC_INDEX_USAGE_STATIC;

    icIndexBuf* ib;
    if (ICEFAIL(m_pDevice->CreateIB(&ib, &index_def)))
    {
        m_pDevice->ReleaseVB(vb);
        delete model;
        icWarningf("Content loader could not create models (%s) index buffer", model_header.modelName);
        return IC_FAIL_GEN;
    }
    icIndexLock indLock;
    if (ICEFAIL(ib->Lock(&indLock)))
    {
        m_pDevice->ReleaseVB(vb);
        m_pDevice->ReleaseIB(ib);
        delete model;
        icWarningf("Content loader could not lock models (%s) index buffer", model_header.modelName);
        return IC_FAIL_GEN;
    }
    uint indsize = (indLock.indexSize == IC_INDEX_SIZE_16) ? 2 : 4;
    if (ICEFAIL(file.Read(indLock.pData, indLock.numIndex*indsize, &sizeread)))
    {
        m_pDevice->ReleaseVB(vb);
        m_pDevice->ReleaseIB(ib);
        delete model;
        icWarningf("Content loader could not read models (%s) index buffer", model_header.modelName);
        return IC_FAIL_GEN;
    }
    ib->Unlock();
    model->m_pIB = ib;


    // CREATE THE DIFFUSE TEXTURE
    icTexture* tex;
    if (strlen(model_header.difTex) >= 5)
    {
        if (ICEOK(Load(model_header.difTex, &tex)))
        {
            model->m_difTex = tex;
        }
    }

    // CREATE THE NORMAL TEXTURE
    if (strlen(model_header.normTex) >= 5)
    {
        if (ICEOK(Load(model_header.normTex, &tex)))
        {
            model->m_normTex = tex;
        }
    }

    // CREATE THE SPECULAR TEXTURE
    if (strlen(model_header.specTex) >= 5)
    {
        if (ICEOK(Load(model_header.specTex, &tex)))
        {
            model->m_specTex = tex;
        }
    }

    *ppObj = model;
    return IC_OK;
}// END FUNCTION Load<icModel>(const char* szFileName, icModel** ppObj)


/*! Model unloader
 *
 *  @param[in/out]  ppObj           Loaded model
 *  @returns        ICRESULT        Success/failure of model unload
**/
template<>
ICRESULT icContentLoader::Unload<icModel>(icModel** ppObj)
{
    if (!*ppObj)
        return IC_OK;

    if (m_pDevice)
    {
        // TODO: reference counting!
        //if (refcount == 0)
        {
            icModel* model = *ppObj;
            m_pDevice->ReleaseTex(model->m_difTex);
            m_pDevice->ReleaseTex(model->m_normTex);
            m_pDevice->ReleaseTex(model->m_specTex);

            m_pDevice->ReleaseVB(model->m_pVB);
            m_pDevice->ReleaseIB(model->m_pIB);
        }

        *ppObj = NULL;
        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION Unload<icTexture>(icTexture** ppObj)
