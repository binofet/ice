#ifndef __IC_VERT_BUFFER_H__
#define __IC_VERT_BUFFER_H__


#include "Core/icGlobals.h"

#include "Math/icColor.h"
#include "Math/Vector/icVector3.h"
#include "Math/Vector/icVector2.h"


#define IC_VERT_USAGE_UNKOWN    0
#define IC_VERT_USAGE_DYNAMIC   1
#define IC_VERT_USAGE_STATIC    2

// enable this to force 32 byte alignment on vert structures
#define ICPADVERTS


/*! Available primitives
**/
enum IC_PRIM
{
    IC_PRIM_TYPE_INVALID=0,
    IC_PRIM_PLIST,              //! Point List
    IC_PRIM_LLIST,              //! Line List
    IC_PRIM_LSTRIP,             //! Line Strip
    IC_PRIM_TLIST,              //! Triangle List
    IC_PRIM_TSTRIP,             //! Triangle Strip
    IC_PRIM_TFAN,               //! Trinalge Fan

    IC_PRIM_FORCE_DWORD = 0x7FFFFFFF, //! Force enumeration compile to 32 bit size
};

/*! Different vertex types
**/
enum IC_VERT
{
    IC_VERT_IVALID=0,
    IC_VERT_2DCOL,
    IC_VERT_2D_DIF,
    IC_VERT_2DCOL_DIF,
    IC_VERT_COL,
    IC_VERT_DIF,
    IC_VERT_MULTITEX2,
    IC_VERT_MULTITEX3,
    IC_VERT_MULTITEX4,
    IC_VERT_NRMCOL,
    IC_VERT_NRMCOLSPC,
    IC_VERT_NRM_DIF,
    IC_VERT_NRMCOL_DIF,
    IC_VERT_NRMCOLSPC_DIF,
    IC_VERT__DIFNRM,
    IC_VERT_COL_DIFNRM,
    IC_VERT__DIFNRMSPC,
    IC_VERT_COL_DIFNRMSPC,

	IC_VERT_DEF_COUNT,

    IC_VERT_FORCE_DWORD = 0x7FFFFFFF, //! Force enumeration compile to 32 bit size
};


/*! Defines paramters for a vertex buffer
**/
struct icVertDef
{
    uint    uiVertSize;     //! Size of each vertex (in bytes)
    uint    uiNumVerts;     //! Number of verticies
    uint    numPrims;       //! The number of primitives
    uchar   usage;          //! How this buffer is to be used
    IC_PRIM primType;       //! Type of primitive
    IC_VERT vertType;
};

/*! Used to lock a vertex buffer
**/
struct icVertLock
{
    uint uiVertSize;        //! Size of verts in locked buffer
    uint uiNumVerts;        //! Number of verts locked
    void* pData;            //! Pointer to locked vertex data
};

//////////////////////////////////////////////////////////////////////////
// ICE VERTEX TYPES

struct ICVRT2DCOL
{
    icVector2   pos;
    icColor     col;
};

struct ICVRT2D_DIF
{
    icVector2   pos;    //8
    icVector2   uv;     //8
};

struct ICVRT2DCOL_DIF
{
    icVector2   pos;    //8
    icColor     col;    //4
    icVector2   uv;     //8
#ifdef ICPADVERTS
    char        padding[12];
#endif
};


struct ICVRTCOL
{
    icVector3   pos;    //12
    icColor     col;    //4
    char        padding[16];
};

struct ICVRT_DIF
{
    icVector3   pos;    //12
    icVector2   uv;     //8
#ifdef ICPADVERTS
    char        padding[12];
#endif
};

struct ICVRT_MULTITEX2
{
    icVector3   pos;
    icVector2   uv1;
    icVector2   uv2;
};

struct ICVRT_MULTITEX3
{
    icVector3   pos;
    icVector2   uv1;
    icVector2   uv2;
    icVector2   uv3;
};

struct ICVRT_MULTITEX4
{
    icVector3   pos;
    icVector2   uv1;
    icVector2   uv2;
    icVector2   uv3;
    icVector2   uv4;
};

struct ICVRTNRMCOL
{
    icVector3   pos;
    icVector3   norm;
    icColor     col;
#ifdef ICPADVERTS
    char        padding[4];
#endif
};

struct ICVRTNRMCOLSPC
{
    icVector3   pos;
    icVector3   norm;
    icColor     col;
    float       spec;
};

struct ICVRTNRM_DIF
{
    icVector3   pos;
    icVector3   norm;
    icVector2   uv;
};


struct ICVRTNRMCOL_DIF
{
    icVector3   pos;
    icVector3   norm;
    icColor     col;
    icVector2   uv;
};

struct ICVRTNRMCOLSPC_DIF
{
    icVector3   pos;
    icVector3   norm;
    icColor     col;
    float       spec;
    icVector2   uv;
};

struct ICVRT_DIFNRM
{
    icVector3   pos;
    icVector2   uvDif;
    icVector2   uvNorm;
};

struct ICVRTCOL_DIFNRM
{
    icVector3   pos;
    icColor     col;
    icVector2   uvDif;
    icVector2   uvNorm;
};

struct ICVRT_DIFNRMSPC
{
    icVector3   pos;
    icVector2   uvDif;
    icVector2   uvNorm;
    icVector2   uvSpec;
};

struct ICVRTCOL_DIFNRMSPC
{
    icVector3   pos;
    icColor     col;
    icVector2   uvDif;
    icVector2   uvNorm;
    icVector2   uvSpec;
};


/*! Provides an interface for vertex buffers
**/
class icVertBuffer
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icVertBuffer( void )
    {
        m_ptrVertBuff = NULL;
        m_bLocked = false;
    };
    virtual ~icVertBuffer( void ){ Cleanup(); };

    virtual ICRESULT Cleanup( void ){ return IC_OK;};

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Lock( icVertLock* pVertLock )=0;
    virtual ICRESULT Unlock( void )=0;

    //////////////////////////////////////////////////////////////////////////
    // ACCESS

    //! Gets a pointer to the vertex buffer object
    //\returns void** Pointer to pointer of vb object
    inline void** GetVertBuff( void ) { return &m_ptrVertBuff; };

    //! Gets a pointer to the vertex buffer definition struct
    //\returns icIndexDef* Pointer to vertex definition
    inline icVertDef* GetVertDef( void ) { return &m_VertDef; };

    //! Queries to see if buffer is currently locked
    //\returns      bool        true if locked.
    inline bool IsLocked(void) {return m_bLocked;};

protected:
    //////////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    void*        m_ptrVertBuff;        //! Pointer to vertex buffer data
    bool         m_bLocked;            //! Tells if vertex buffer is locked
    icVertDef    m_VertDef;            //! Defines this vertex buffer
};

#endif //__ICE_VERT_BUFFER_H__

