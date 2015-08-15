#ifndef __IC_INDEX_SIZEFER_H__
#define __IC_INDEX_SIZEFER_H__

#include "Core/icGlobals.h"

#define IC_INDEX_USAGE_UNKOWN    0
#define IC_INDEX_USAGE_DYNAMIC   1
#define IC_INDEX_USAGE_STATIC    2

enum IC_INDEX_SIZE
{
    IC_INDEX_SIZE_UNKOWN=0,
    IC_INDEX_SIZE_16,
    IC_INDEX_SIZE_32
};

struct icIndexDef
{
    uchar           usage;
    uint            numIndex;
    IC_INDEX_SIZE   indexSize;
};


struct icIndexLock
{
    uint            numIndex;
    IC_INDEX_SIZE    indexSize;
    void*            pData;
};

class icIndexBuf
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icIndexBuf( void )
    {
        m_pIndexBuf = NULL;
        m_bLocked = false;
    };
    virtual ~icIndexBuf( void ) { Cleanup(); };

    virtual ICRESULT Cleanup( void ) { return IC_OK; };

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock( icIndexLock* pIndexLock )=0;
    virtual ICRESULT Unlock( void )=0;

    //////////////////////////////////////////////////////////////////////////
    // ACCESS

    //! Gets the index buffer object
    //\returns void** Pointer to pointer of index object
    inline void** GetIndexBuffer( void ) { return &m_pIndexBuf; };

    //! Gets a pointer to the index buffer definition struct
    //\returns icIndexDef* Pointer to index definition
    inline icIndexDef* GetIndexDef( void ) { return &m_IndexDef; };

protected:
    void*            m_pIndexBuf;    //! Pointer to index buffer data
    bool            m_bLocked;        //! Tells if index buffer is locked
    icIndexDef        m_IndexDef;        //! Defines this index buffer
};

#endif //__IC_INDEX_SIZEFER_H__

