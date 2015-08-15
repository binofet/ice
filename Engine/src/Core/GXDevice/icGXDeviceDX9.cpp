
#include "Core/GXDevice/icGXDeviceDX9.h"


#if (defined(WIN32) && defined(ICDX9))

#include "Core/Application/Window/icWindowWIN.h"

#include "Render/VertBuffer/icVertBufferDX9.h"
#include "Render/IndexBuffer/icIndexBufDX9.h"
#include "Render/Texture/icTextureDX9.h"

/*! Default Constructor
**/
icGXDeviceDX9::icGXDeviceDX9( void )
{
    m_colClear = icColor::BMAGENTA;
    m_pd3d = NULL;
    m_pDevice = NULL;
    m_D3DLibrary = NULL;
    m_D3DUtilLib = NULL;
    m_CreateD3DFunc = NULL;
    m_CreateTexFunc = NULL;
}// END FUNCTION icGXDeviceDX9( void )


/*! Default Destructor
**/
icGXDeviceDX9::~icGXDeviceDX9( void )
{
    Cleanup();
    if (m_D3DLibrary)
    {
        FreeLibrary(m_D3DLibrary);
    }
    if (m_D3DUtilLib)
    {
        FreeLibrary(m_D3DUtilLib);
    }
}// END FUNCTION ~icGXDeviceDX9( void )


/*! Cleans up the device, frees any allocated memory
 *
 *    @returns    ICRESULT    Result of cleanup
 *
**/
ICRESULT icGXDeviceDX9::Cleanup( void )
{
    if (m_pDevice)
    {
        m_pDevice->Release();
        m_pDevice = NULL;
    }
    if (m_pd3d)
    {
        m_pd3d->Release();
        m_pd3d = NULL;
    }
    return IC_OK;
}// END FUNCTION Cleanup( void )


/*! Initializes the DirectX9 device
 *
 *    @param[in]    hWnd        Pointer to the window
 *    @returns    ICRESULT    Status of initialization
 *
**/
ICRESULT icGXDeviceDX9::Init(class icWindow* const pWindow)
{
    // we can only create a device with a handle to the window
    if ( pWindow )
    {
        m_pMainWindow = pWindow;
        return CreateDevice();
    }

    return IC_FAIL_GEN;
}// END FUNCTION Init( void* hWnd )


/*! Creates the dx9 device
 *
 *    @returns    ICRESULT    Result of device creation
 *
**/
ICRESULT icGXDeviceDX9::CreateDevice()
{
    Cleanup();

    // Load the d3d library and get the proc address for Direct3DCreate9
    if (!m_D3DLibrary)
    {
#ifdef DEBUG
        m_D3DLibrary = LoadLibrary( "d3d9d.dll" );
#else
        m_D3DLibrary = LoadLibrary( "d3d9.dll" );
#endif
        if (!m_D3DLibrary)
        {
            icError("Failed to load d3d9.dll.");
            return IC_FAIL_CREATE_DEVICE;
        }

        m_CreateD3DFunc = (D3DCREATEDEV) GetProcAddress(m_D3DLibrary,
                                                            "Direct3DCreate9");

        if (!m_CreateD3DFunc)
        {
            icError("Failed to get proc adress of Direct3DCreate9.");
            return IC_FAIL_CREATE_DEVICE;
        }
    }

    // Load the dxutil library
    if (!m_D3DUtilLib)
    {
        char libName[16];
        int latestrev = D3DX_LATEST;
        while (!m_D3DUtilLib && latestrev > 24)
        {
#ifdef DEBUG
#ifdef _MSC_VER
            sprintf_s(libName, 16, "D3DX9d_%i.dll", latestrev--);
#else
            sprintf(libName, "D3DX9_%id.dll", latestrev);
#endif
#else
#ifdef _MSC_VER
            sprintf_s(libName, 16, "D3DX9_%i.dll", latestrev);
#else
            sprintf(libName, "D3DX9_%i.dll", latestrev);
#endif
#endif
            m_D3DUtilLib = LoadLibrary( libName );
        }

        if (!m_D3DUtilLib)
        {
            icError("Failed to load a suitible d3dx9_##.dll");
            return IC_FAIL_CREATE_DEVICE;
        }

        m_CreateTexFunc = (D3DTEXFROMFILE) GetProcAddress(m_D3DUtilLib,
                                            "D3DXCreateTextureFromFileA");

        if (!m_CreateTexFunc)
        {
            icError(\
            "Failed to get proc adress of D3DXCreateTextureFromFileA.");
            return IC_FAIL_CREATE_DEVICE;
        }

    }


    // Initialize dx9
    m_pd3d = (*m_CreateD3DFunc)(D3D_SDK_VERSION);
    if (!m_pd3d)
    {
        return IC_FAIL_CREATE_DEVICE;
    }

    icWindowWIN* pWin = (icWindowWIN*)m_pMainWindow;
    icWindowDef winDef;
    m_pMainWindow->GetParams(&winDef);

    D3DPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(d3dpp));
    d3dpp.Windowed = !winDef.bFullScreen;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    if (winDef.bFullScreen)
    {
        D3DDISPLAYMODE dMode;
        m_pd3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dMode);
        d3dpp.BackBufferHeight = dMode.Height;
        d3dpp.BackBufferWidth = dMode.Width;
        d3dpp.hDeviceWindow = (HWND)m_pMainWindow;
        d3dpp.BackBufferFormat=D3DFMT_X8R8G8B8;
    }
    // Create the device
    if (FAILED(m_pd3d->CreateDevice(D3DADAPTER_DEFAULT,
                                    D3DDEVTYPE_HAL,
                                    pWin->GetHWND(),
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &d3dpp,
                                    &m_pDevice)))
    {
        m_pd3d->Release();
        m_pd3d=NULL;
        m_pDevice=NULL;
        return IC_FAIL_CREATE_DEVICE;
    }


    // Turn on ambient lighting
    m_pDevice->SetRenderState( D3DRS_AMBIENT, 0xffffffff );

	// IC_VERT_2DCOL
	D3DVERTEXELEMENT9 decl2DCOL[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 8, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	HRESULT hr = m_pDevice->CreateVertexDeclaration( decl2DCOL, &_vDecl[IC_VERT_2DCOL] );

	//IC_VERT_2D_DIF
	D3DVERTEXELEMENT9 decl2D_DIF[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl2D_DIF, &_vDecl[IC_VERT_2D_DIF] );

	//IC_VERT_2DCOL_DIF
	D3DVERTEXELEMENT9 decl2DCOL_DIF[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 8, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
		};
	hr = m_pDevice->CreateVertexDeclaration( decl2DCOL_DIF, &_vDecl[IC_VERT_2DCOL_DIF] );

	//IC_VERT_COL
	D3DVERTEXELEMENT9 decl3D_COL[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_COL, &_vDecl[IC_VERT_COL] );

	//IC_VERT_DIF
	D3DVERTEXELEMENT9 decl3D_DIF[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_DIF, &_vDecl[IC_VERT_DIF] );

	//IC_VERT_MULTITEX2
	D3DVERTEXELEMENT9 decl3D_M2[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_M2, &_vDecl[IC_VERT_MULTITEX2] );

	//IC_VERT_MULTITEX3
	D3DVERTEXELEMENT9 decl3D_M3[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 2},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_M3, &_vDecl[IC_VERT_MULTITEX3] );

	//IC_VERT_MULTITEX4
	D3DVERTEXELEMENT9 decl3D_M4[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 2},
		{0, 36, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 3},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_M4, &_vDecl[IC_VERT_MULTITEX4] );

	//IC_VERT_NRMCOL
	D3DVERTEXELEMENT9 decl3D_NRMCOL[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
		{0, 24, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_NRMCOL, &_vDecl[IC_VERT_NRMCOL] );

	//IC_VERT_NRMCOLSPC
	D3DVERTEXELEMENT9 decl3D_NRMCOLSPC[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
		{0, 24, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0, 28, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 1},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_NRMCOLSPC, &_vDecl[IC_VERT_NRMCOLSPC] );

	//IC_VERT_NRM_DIF
	D3DVERTEXELEMENT9 decl3D_NRM_DIFF[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
		{0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_NRM_DIFF, &_vDecl[IC_VERT_NRM_DIF] );

	//IC_VERT_NRMCOLSPC_DIF
	D3DVERTEXELEMENT9 decl3D_NRMCOLSPC_DIF[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL, 0},
		{0, 24, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0, 28, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 1},
		{0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_NRMCOLSPC_DIF, &_vDecl[IC_VERT_NRMCOLSPC_DIF] );

	//IC_VERT__DIFNRM
	D3DVERTEXELEMENT9 decl3D__DIFNRM[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D__DIFNRM, &_vDecl[IC_VERT__DIFNRM] );

	//IC_VERT_COL_DIFNRM
	D3DVERTEXELEMENT9 decl3D_COL_DIFNRM[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_COL_DIFNRM, &_vDecl[IC_VERT_COL_DIFNRM] );

	//IC_VERT__DIFNRMSPC
	D3DVERTEXELEMENT9 decl3D__DIFNRMSPC[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 2},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D__DIFNRMSPC, &_vDecl[IC_VERT__DIFNRMSPC] );

	//IC_VERT_COL_DIFNRMSPC
	D3DVERTEXELEMENT9 decl3D_COL_DIFNRMSPC[] = 
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR, 0},
		{0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0},
		{0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 1},
		{0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 2},
		{0xFF,0,D3DDECLTYPE_UNUSED, 0,0,0}// D3DDECL_END 
	};
	hr = m_pDevice->CreateVertexDeclaration( decl3D_COL_DIFNRMSPC, &_vDecl[IC_VERT_COL_DIFNRMSPC] );

	return IC_OK;
}// END FUNCTION CreateDevice( void )


/*! Updates various stats in the engine
 *
 *    @returns    ICRESULT    Result of update
 *
**/
ICRESULT icGXDeviceDX9::Update( void )
{
    return IC_OK;
}// END FUNCTION Update( void )


/*! Clears the backbuffer and starts a new render frame
**/
void icGXDeviceDX9::FrameBegin( void )
{
    if (m_pDevice)
    {
        m_pDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                            m_colClear.A8R8G8B8(), 1.0f, 0);
        m_pDevice->BeginScene();
    }
}// END FUNCTION FrameBegin( void )


/*! Presents the current frame to the display
**/
void icGXDeviceDX9::FrameEnd(bool bSwap)
{
    if (m_pDevice)
    {
        m_pDevice->EndScene();
        m_pDevice->Present( NULL, NULL, NULL, NULL );
    }
}// END FUNCTION FrameEnd( void )


/*! Renders a vertex buffer to the current frame
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @returns    ICRESULT    Result of rendering the vert buffer
 *
**/
ICRESULT icGXDeviceDX9::DrawVertBuf(class icVertBuffer* pVB, uint offset,
                                    uint count)
{
	assert(pVB);
	if (m_pDevice)
	{
		HRESULT hr = S_OK;
		DX9VB* data = (DX9VB*)(*pVB->GetVertBuff());
		icVertDef* pvDef = pVB->GetVertDef();
		uint offset = 0;
		uint stride = pvDef->uiVertSize;

		hr = m_pDevice->SetVertexDeclaration(_vDecl[pvDef->vertType]);

		hr = m_pDevice->SetStreamSource(0, data, offset, stride);

		D3DPRIMITIVETYPE primType = (D3DPRIMITIVETYPE)pvDef->primType;
		uint startvert = 0;
		hr |= m_pDevice->DrawPrimitive( primType, startvert, pvDef->numPrims );

		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}// END FUNCTION DrawVertBuf( icVertBuffer*, icMaterial* )


/*! Renders an indexed vertex buffer to the current frame
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @param[in]    pIB            Pointer to the index buffer
 *    @returns    ICRESULT    Result of rendering the vert buffer
 *
**/
ICRESULT icGXDeviceDX9::DrawIndexedVertBuf(    class icVertBuffer* pVB,
                                            class icIndexBuf* pIB )
{
    assert(pVB && pIB);
    if (m_pDevice)
    {
        DX9VB* data = (DX9VB*)(*pVB->GetVertBuff());
        icVertDef* pvDef = pVB->GetVertDef();
        uint offset = 0;
        uint stride = pvDef->uiVertSize;
        
		HRESULT hr = m_pDevice->SetVertexDeclaration(_vDecl[pvDef->vertType]);
		
		hr = m_pDevice->SetStreamSource(0, data, offset, stride);

		hr = m_pDevice->SetIndices(((icIndexBufDX9*)pIB)->GetIB());

        D3DPRIMITIVETYPE primType = (D3DPRIMITIVETYPE)pvDef->primType;
        uint startvert = 0;
        uint minIndex = 0;
        uint numVerts = pvDef->uiNumVerts;
        uint startIndex = 0;
        uint numPrims = pvDef->numPrims;
        hr = m_pDevice->DrawIndexedPrimitive(primType, startvert, minIndex, numVerts, startIndex, numPrims );

        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION DrawIndexedVertBuf( icVertBuffer*, icIndexBuf*, icMaterial* )


/*! Creates a dx9 vertex buffer
 *
 *    @param[out]    ppVB        Pointer to ouput VB pointer
 *    @param[in]    pVertDef    Information about the VB
 *    @returns    ICRESULT    Result of creating VB
 *
**/
ICRESULT icGXDeviceDX9::CreateVB( class icVertBuffer** ppVB,
                                struct icVertDef* pVertDef )
{
    assert( ppVB && pVertDef );
    if (m_pDevice)
    {
        icVertBufferDX9* vb = new icVertBufferDX9();
        if (vb)
        {
            uint length = pVertDef->uiVertSize * pVertDef->uiNumVerts;
            //! Todo, expose these
            DWORD usage = D3DUSAGE_WRITEONLY;
            D3DPOOL pool = D3DPOOL_DEFAULT;
            if (FAILED(m_pDevice->CreateVertexBuffer( length, usage,
                        0, pool,
                        (IDirect3DVertexBuffer9**)vb->GetVertBuff(), NULL )))
            {
                delete vb;
                *ppVB = NULL;
                return IC_FAIL_CREATE_VB;
            }
            memcpy(vb->GetVertDef(), pVertDef, sizeof(icVertDef) );
            *ppVB = vb;
            return IC_OK;
        }
        else
            return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateVB( class icVertBuffer** ppVB )


/*! Releases vertex buffer memory back to graphics card
 *
 *    @param[in]    pVB            Pointer to VB to release
 *    @returns    ICRESULT    Result of releasing VB
 *
**/
ICRESULT icGXDeviceDX9::ReleaseVB( class icVertBuffer* pVB )
{
    delete pVB;
    return IC_OK;
}// END FUNCTION ReleaseVB( class icVertBuffer* pVB )


/*! Creates a dx9 index buffer
 *
 *    @param[out]    ppIB        Pointer to ouput IB pointer
 *    @param[in]    pIndexDef    Information about the IB
 *    @returns    ICRESULT    Result of creating IB
 *
**/
ICRESULT icGXDeviceDX9::CreateIB( class icIndexBuf** ppIB,
                                struct icIndexDef* pIndexDef )
{
    assert( ppIB && pIndexDef );
    if (m_pDevice)
    {
        icIndexBufDX9* ib = new icIndexBufDX9();
        if (ib)
        {
            uchar size = 2;
            D3DFORMAT format = D3DFMT_INDEX16;
            if (pIndexDef->indexSize == IC_INDEX_SIZE_32)
            {
                size = 4;
                format = D3DFMT_INDEX32;
            }
            uint length = pIndexDef->numIndex * size;
            DWORD usage = D3DUSAGE_WRITEONLY;
            D3DPOOL pool = D3DPOOL_DEFAULT;
            if(FAILED(m_pDevice->CreateIndexBuffer( length, usage,
                                                    format, pool,
                (IDirect3DIndexBuffer9**)ib->GetIndexBuffer(), NULL )))
            {
                delete ib;
                *ppIB = NULL;
                return IC_FAIL_CREATE_IB;
            }
            memcpy(ib->GetIndexDef(), pIndexDef, sizeof(icIndexDef));
            *ppIB = ib;
            return IC_OK;
        }
        else
            return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateIB( icIndexBuf**, icIndexDef* )


/*! Releases index buffer from memory
 *
 *    @param[in]    pIB            Pointer to index buffer
 *    @returns    ICRESULT    Result of index buffer release
 *
**/
ICRESULT icGXDeviceDX9::ReleaseIB( class icIndexBuf* pVB )
{
    delete pVB;
    return IC_OK;
}// END FUNCTION ReleaseIB( class icIndexBuf* pIB )


/*! Creates a dx9 texture
 *
 *    @param[out]    ppTex        Pointer to ouput texture pointer
 *    @param[in]    pTexDef        Information about the texture
 *    @returns    ICRESULT    Result of creating texture
 *
**/
ICRESULT icGXDeviceDX9::CreateTex( class icTexture** ppTex,
                                 struct icTexDef* pTexDef)
{
    assert( ppTex && pTexDef );
    if (m_pDevice)
    {
        icTextureDX9* tex = new icTextureDX9();
        if (tex)
        {
            DWORD usage = (pTexDef->usage) ? D3DUSAGE_DYNAMIC : 0;
            D3DFORMAT format = D3DFMT_A8R8G8B8;
            D3DPOOL pool =  D3DPOOL_DEFAULT;
            if (FAILED(m_pDevice->CreateTexture(pTexDef->width,
                pTexDef->height,pTexDef->mipLevels, usage, format,
                    pool, (DX9TEX**)tex->GetTexture(), NULL)))
            {
                delete tex;
                *ppTex = NULL;
                return IC_FAIL_CREATE_TEX;
            }
            memcpy(tex->GetTexDef(), pTexDef, sizeof(icTexDef));
            *ppTex = tex;
            return IC_OK;
        }
        else
            return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateTex( class icTextue**, struct icTexDef )


/*! Creates an icTexture from a file
 *
 *    @param[in/out]    ppTex        Pointer to ouput texture pointer
 *    @param[in]        szFile        Name of file to load as texture
 *    @returns        ICRESULT    Result of texture creation
 *
**/
ICRESULT icGXDeviceDX9::CreateTexFromFile( class icTexture** ppTex,
                                         const char* szFile )
{
    assert( ppTex && szFile);
    if (m_pDevice)
    {
        icFileName file;
        file.ParseFileName( szFile );
        const char* ext = file.GetFileExtension();
        if (ext)
        {
            // Direct x supports these files
            if (!strcmp(ext, ".bmp") ||
                !strcmp(ext, ".dds") ||
                !strcmp(ext, ".dib") ||
                !strcmp(ext, ".hdr") ||
                !strcmp(ext, ".jpg") ||
                !strcmp(ext, ".pfm") ||
                !strcmp(ext, ".png") ||
                !strcmp(ext, ".ppm") ||
                !strcmp(ext, ".tga") )
            {
                icTextureDX9* tex = new icTextureDX9();
                if (tex)
                {
                    if (FAILED((*m_CreateTexFunc)( m_pDevice,
                                                   szFile,
                                    (DX9TEX**)tex->GetTexture() )))
                    {
                        delete tex;
                        return IC_FAIL_CREATE_TEX;
                    }
					*ppTex = tex;
                    return IC_OK;
                }
                else
                    return IC_FAIL_OUT_OF_MEM;
            }
            return IC_FAIL_FILE_NOT_SUPPORTED;
        }
        return IC_FAIL_FILE_NOT_SUPPORTED;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION CreateTexFromFile( class icTexutre** ppTex )


/*! Releases a texture from graphics memory
 *
 *    @param[in]    pTex        Pointer to texture to release
 *    returns        ICRESULT    Result of texture release
 *
**/
ICRESULT icGXDeviceDX9::ReleaseTex( class icTexture *pTex )
{
    delete pTex;
    return IC_OK;
}// END FUNCTION ReleaseTex( class icTexture* pTex )


ICRESULT icGXDeviceDX9::CreateFont(class icFontI** ppIFont,
                                   struct icFontParams* pFontParams)
{
	return IC_OK;
}

ICRESULT icGXDeviceDX9::ReleaseFont( class icFontI* ppIFont )
{
	return IC_OK;
}

ICRESULT icGXDeviceDX9::EnableAlphaBlend(bool bAlphaBlend)
{
	if (m_pDevice)
	{
		m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

ICRESULT icGXDeviceDX9::EnableDepthTest(bool bDepth)
{
	if (m_pDevice)
	{
		if (bDepth)
		{
			m_pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			//m_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		}
		else
		{
			m_pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			//m_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		}
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

ICRESULT icGXDeviceDX9::EnableFog(bool bEnable, const icFogParams& params)
{
	return IC_OK;
}

/*! Sets the ambient light color
 *
 *    @param        col            Ambient light color
 *    @returns    ICRESULT    Result of state change
 *
**/
ICRESULT icGXDeviceDX9::SetAmbientColor( const icColor& col )
{
    //ID3DXEffect* ef;
    //ef->Set
    return IC_OK;
}// END FUNCTION SetAmbientColor( const icColor& col )

ICRESULT icGXDeviceDX9::SetClearColor(const icColor& col)
{
	m_colClear = col;
	return IC_OK;
}

/*! Sets the device triangle cull mode
 *
 *    @param[in]    cMode_e        Tells the winding order for culling
 *    @returns    ICRESULT    Result of cull mode change request
 *
**/
ICRESULT icGXDeviceDX9::SetCullMode( IC_CULL_MODE cMode_e )
{
    if (m_pDevice)
    {
        m_pDevice->SetRenderState(D3DRS_CULLMODE, (D3DCULL)(cMode_e+1));
        return IC_OK;
    }
    return IC_FAIL_NO_DEVICE;
}// END FUNCTION SetCullMode( IC_CULL_MODE cMode_e )


/*! Sets the projection matrix
 *
 *    @param        proj        The current projection matrix
 *    @returns    ICRESULT    Result of the state change
 *
**/
ICRESULT icGXDeviceDX9::SetProjection( const icMatrix44& proj )
{
	if (m_pDevice)
	{
		m_pDevice->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)(const icReal*)proj);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}// END FUNCTION SetProjection( const icMatrix44& proj )


/*! Sets the view matrix
 *
 *    @param        view        The current view matrix
 *    @returns    ICRESULT    Result of the state change
 *
**/
ICRESULT icGXDeviceDX9::SetViewMatrix( const icMatrix44& view )
{
	if (m_pDevice)
	{
		m_pDevice->SetTransform(D3DTS_VIEW, (const D3DMATRIX*)(const icReal*)view);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}// END FUNCTION SetViewMatrix( const icMatrix44& view )


/*! Set the world transform matrix
 *
 *    @param        world        The current world transform
 *    @returns    ICRESULT    Result of the state change
 *
**/
ICRESULT icGXDeviceDX9::SetWorldTransform( const icMatrix44& world )
{
	if (m_pDevice)
	{
		m_pDevice->SetTransform(D3DTS_WORLD, (const D3DMATRIX*)(const icReal*)world);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}// END FUNCTION SetWorldTransform( const icTransform& world )

ICRESULT icGXDeviceDX9::SetTexture(uint index, icTexture* pTex)
{
	if (!pTex)
	{
		return IC_WARN_GEN;
	}
	if (m_pDevice)
	{
		m_pDevice->SetTexture(index, ((icTextureDX9*)pTex)->GetTex());
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! This may be un-nessecary
 *
**/
void icGXDeviceDX9::ChangeSize( ushort sizeX, ushort sizeY )
{
}// END FUNCTION ChangeSize( ushort sizeX, ushort sizeY )

#endif //#if (defined(_WIN32) && defined(ICDX9))
