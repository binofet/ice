
#include "ice/render/gx_device_dx9.h"
namespace ice::render {

#if (defined(WIN32) && defined(ICDX9))
} // namespace ice::render

#include "ice/platform/win32/window.h"

#include "ice/render/index_buf_dx9.h"
#include "ice/render/texture_dx9.h"
#include "ice/render/vert_buffer_dx9.h"
namespace ice::render {

/*! Default Constructor
 **/
gx_device_dx9::gx_device_dx9(void) {
	m_colClear      = ice::math::color::BMAGENTA;
	m_pd3d          = NULL;
	m_pDevice       = NULL;
	m_D3DLibrary    = NULL;
	m_D3DUtilLib    = NULL;
	m_CreateD3DFunc = NULL;
	m_CreateTexFunc = NULL;
} // END FUNCTION icGXDeviceDX9( void )

/*! Default Destructor
 **/
gx_device_dx9::~gx_device_dx9(void) {
	Cleanup();
	if (m_D3DLibrary) {
		FreeLibrary(m_D3DLibrary);
	}
	if (m_D3DUtilLib) {
		FreeLibrary(m_D3DUtilLib);
	}
} // END FUNCTION ~icGXDeviceDX9( void )

/*! Cleans up the device, frees any allocated memory
 *
 *    @returns    ICRESULT    Result of cleanup
 *
 **/
ice::result gx_device_dx9::Cleanup(void) {
	if (m_pDevice) {
		m_pDevice->Release();
		m_pDevice = NULL;
	}
	if (m_pd3d) {
		m_pd3d->Release();
		m_pd3d = NULL;
	}
	return IC_OK;
} // END FUNCTION Cleanup( void )

/*! Initializes the DirectX9 device
 *
 *    @param[in]    hWnd        Pointer to the window
 *    @returns    ICRESULT    Status of initialization
 *
 **/
ice::result gx_device_dx9::Init(class ice::platform::window *const pWindow) {
	// we can only create a device with a handle to the window
	if (pWindow) {
		m_pMainWindow = pWindow;
		return CreateDevice();
	}

	return IC_FAIL_GEN;
} // END FUNCTION Init( void* hWnd )

/*! Creates the dx9 device
 *
 *    @returns    ICRESULT    Result of device creation
 *
 **/
ice::result gx_device_dx9::CreateDevice() {
	Cleanup();

	// Load the d3d library and get the proc address for Direct3DCreate9
	if (!m_D3DLibrary) {
#ifdef DEBUG
		m_D3DLibrary = LoadLibrary("d3d9d.dll");
#else
		m_D3DLibrary = LoadLibrary("d3d9.dll");
#endif
		if (!m_D3DLibrary) {
			ice::error("Failed to load d3d9.dll.");
			return IC_FAIL_CREATE_DEVICE;
		}

		m_CreateD3DFunc = (D3DCREATEDEV)GetProcAddress(m_D3DLibrary, "Direct3DCreate9");

		if (!m_CreateD3DFunc) {
			ice::error("Failed to get proc adress of Direct3DCreate9.");
			return IC_FAIL_CREATE_DEVICE;
		}
	}

	// Load the dxutil library
	if (!m_D3DUtilLib) {
		char libName[16];
		int  latestrev = D3DX_LATEST;
		while (!m_D3DUtilLib && latestrev > 24) {
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
			m_D3DUtilLib = LoadLibrary(libName);
		}

		if (!m_D3DUtilLib) {
			ice::error("Failed to load a suitible d3dx9_##.dll");
			return IC_FAIL_CREATE_DEVICE;
		}

		m_CreateTexFunc =
		    (D3DTEXFROMFILE)GetProcAddress(m_D3DUtilLib, "D3DXCreateTextureFromFileA");

		if (!m_CreateTexFunc) {
			ice::error("Failed to get proc adress of D3DXCreateTextureFromFileA.");
			return IC_FAIL_CREATE_DEVICE;
		}
	}

	// Initialize dx9
	m_pd3d = (*m_CreateD3DFunc)(D3D_SDK_VERSION);
	if (!m_pd3d) {
		return IC_FAIL_CREATE_DEVICE;
	}

	ice::platform::win32::window *pWin = (ice::platform::win32::window *)m_pMainWindow;
	ice::platform::window_def     winDef;
	m_pMainWindow->GetParams(&winDef);

	D3DPRESENT_PARAMETERS d3dpp;
	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed               = !winDef.bFullScreen;
	d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	if (winDef.bFullScreen) {
		D3DDISPLAYMODE dMode;
		m_pd3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &dMode);
		d3dpp.BackBufferHeight = dMode.Height;
		d3dpp.BackBufferWidth  = dMode.Width;
		d3dpp.hDeviceWindow    = (HWND)m_pMainWindow;
		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	}
	// Create the device
	if (FAILED(m_pd3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pWin->GetHWND(),
	                                D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &m_pDevice))) {
		m_pd3d->Release();
		m_pd3d    = NULL;
		m_pDevice = NULL;
		return IC_FAIL_CREATE_DEVICE;
	}

	// Turn on ambient lighting
	m_pDevice->SetRenderState(D3DRS_AMBIENT, 0xffffffff);

	// IC_VERT_2DCOL
	D3DVERTEXELEMENT9 decl2DCOL[] = {
	    {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 8, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	HRESULT hr = m_pDevice->CreateVertexDeclaration(decl2DCOL, &_vDecl[ic_vert_2_dcol]);

	// IC_VERT_2D_DIF
	D3DVERTEXELEMENT9 decl2D_DIF[] = {
	    {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl2D_DIF, &_vDecl[ic_vert_2_d_dif]);

	// IC_VERT_2DCOL_DIF
	D3DVERTEXELEMENT9 decl2DCOL_DIF[] = {
	    {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 8, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl2DCOL_DIF, &_vDecl[ic_vert_2_dcol_dif]);

	// IC_VERT_COL
	D3DVERTEXELEMENT9 decl3D_COL[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_COL, &_vDecl[ic_vert_col]);

	// IC_VERT_DIF
	D3DVERTEXELEMENT9 decl3D_DIF[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_DIF, &_vDecl[ic_vert_dif]);

	// IC_VERT_MULTITEX2
	D3DVERTEXELEMENT9 decl3D_M2[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_M2, &_vDecl[ic_vert_multitex2]);

	// IC_VERT_MULTITEX3
	D3DVERTEXELEMENT9 decl3D_M3[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_M3, &_vDecl[ic_vert_multitex3]);

	// IC_VERT_MULTITEX4
	D3DVERTEXELEMENT9 decl3D_M4[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
	    {0, 36, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_M4, &_vDecl[ic_vert_multitex4]);

	// IC_VERT_NRMCOL
	D3DVERTEXELEMENT9 decl3D_NRMCOL[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	    {0, 24, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_NRMCOL, &_vDecl[ic_vert_nrmcol]);

	// IC_VERT_NRMCOLSPC
	D3DVERTEXELEMENT9 decl3D_NRMCOLSPC[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	    {0, 24, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0, 28, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_NRMCOLSPC, &_vDecl[ic_vert_nrmcolspc]);

	// IC_VERT_NRM_DIF
	D3DVERTEXELEMENT9 decl3D_NRM_DIFF[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	    {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_NRM_DIFF, &_vDecl[ic_vert_nrm_dif]);

	// IC_VERT_NRMCOLSPC_DIF
	D3DVERTEXELEMENT9 decl3D_NRMCOLSPC_DIF[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
	    {0, 24, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0, 28, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1},
	    {0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_NRMCOLSPC_DIF, &_vDecl[ic_vert_nrmcolspc_dif]);

	// IC_VERT__DIFNRM
	D3DVERTEXELEMENT9 decl3D__DIFNRM[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D__DIFNRM, &_vDecl[ic_vert__difnrm]);

	// IC_VERT_COL_DIFNRM
	D3DVERTEXELEMENT9 decl3D_COL_DIFNRM[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_COL_DIFNRM, &_vDecl[ic_vert_col_difnrm]);

	// IC_VERT__DIFNRMSPC
	D3DVERTEXELEMENT9 decl3D__DIFNRMSPC[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D__DIFNRMSPC, &_vDecl[ic_vert__difnrmspc]);

	// IC_VERT_COL_DIFNRMSPC
	D3DVERTEXELEMENT9 decl3D_COL_DIFNRMSPC[] = {
	    {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
	    {0, 12, D3DDECLTYPE_UBYTE4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
	    {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
	    {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
	    {0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
	    {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0} // D3DDECL_END
	};
	hr = m_pDevice->CreateVertexDeclaration(decl3D_COL_DIFNRMSPC, &_vDecl[ic_vert_col_difnrmspc]);

	return IC_OK;
} // END FUNCTION CreateDevice( void )

/*! Updates various stats in the engine
 *
 *    @returns    ICRESULT    Result of update
 *
 **/
ice::result gx_device_dx9::Update(void) { return IC_OK; } // END FUNCTION Update( void )

/*! Clears the backbuffer and starts a new render frame
 **/
void gx_device_dx9::FrameBegin(void) {
	if (m_pDevice) {
		m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, m_colClear.A8R8G8B8(), 1.0f,
		                 0);
		m_pDevice->BeginScene();
	}
} // END FUNCTION FrameBegin( void )

/*! Presents the current frame to the display
 **/
void gx_device_dx9::FrameEnd(bool bSwap) {
	if (m_pDevice) {
		m_pDevice->EndScene();
		m_pDevice->Present(NULL, NULL, NULL, NULL);
	}
} // END FUNCTION FrameEnd( void )

/*! Renders a vertex buffer to the current frame
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @returns    ICRESULT    Result of rendering the vert buffer
 *
 **/
ice::result gx_device_dx9::DrawVertBuf(class vert_buffer *pVB, ice::uint offset, ice::uint count) {
	assert(pVB);
	if (m_pDevice) {
		HRESULT   hr     = S_OK;
		DX9VB    *data   = (DX9VB *)(*pVB->GetVertBuff());
		vert_def *pvDef  = pVB->GetVertDef();
		ice::uint offset = 0;
		ice::uint stride = pvDef->uiVertSize;

		hr = m_pDevice->SetVertexDeclaration(_vDecl[pvDef->vertType]);

		hr = m_pDevice->SetStreamSource(0, data, offset, stride);

		D3DPRIMITIVETYPE primType  = (D3DPRIMITIVETYPE)pvDef->primType;
		ice::uint        startvert = 0;
		hr |= m_pDevice->DrawPrimitive(primType, startvert, pvDef->numPrims);

		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION DrawVertBuf( icVertBuffer*, icMaterial* )

/*! Renders an indexed vertex buffer to the current frame
 *
 *    @param[in]    pVB            Pointer to the vertex buffer
 *    @param[in]    pIB            Pointer to the index buffer
 *    @returns    ICRESULT    Result of rendering the vert buffer
 *
 **/
ice::result gx_device_dx9::DrawIndexedVertBuf(class vert_buffer *pVB, class index_buf *pIB) {
	assert(pVB && pIB);
	if (m_pDevice) {
		DX9VB    *data   = (DX9VB *)(*pVB->GetVertBuff());
		vert_def *pvDef  = pVB->GetVertDef();
		ice::uint offset = 0;
		ice::uint stride = pvDef->uiVertSize;

		HRESULT hr = m_pDevice->SetVertexDeclaration(_vDecl[pvDef->vertType]);

		hr = m_pDevice->SetStreamSource(0, data, offset, stride);

		hr = m_pDevice->SetIndices(((index_buf_dx9 *)pIB)->GetIB());

		D3DPRIMITIVETYPE primType   = (D3DPRIMITIVETYPE)pvDef->primType;
		ice::uint        startvert  = 0;
		ice::uint        minIndex   = 0;
		ice::uint        numVerts   = pvDef->uiNumVerts;
		ice::uint        startIndex = 0;
		ice::uint        numPrims   = pvDef->numPrims;
		hr = m_pDevice->DrawIndexedPrimitive(primType, startvert, minIndex, numVerts, startIndex,
		                                     numPrims);

		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION DrawIndexedVertBuf( icVertBuffer*, icIndexBuf*, icMaterial* )

/*! Creates a dx9 vertex buffer
 *
 *    @param[out]    ppVB        Pointer to ouput VB pointer
 *    @param[in]    pVertDef    Information about the VB
 *    @returns    ICRESULT    Result of creating VB
 *
 **/
ice::result gx_device_dx9::CreateVB(class vert_buffer **ppVB, struct vert_def *pVertDef) {
	assert(ppVB && pVertDef);
	if (m_pDevice) {
		vert_buffer_dx9 *vb = new vert_buffer_dx9();
		if (vb) {
			ice::uint length = pVertDef->uiVertSize * pVertDef->uiNumVerts;
			//! Todo, expose these
			DWORD   usage = D3DUSAGE_WRITEONLY;
			D3DPOOL pool  = D3DPOOL_DEFAULT;
			if (FAILED(m_pDevice->CreateVertexBuffer(
			        length, usage, 0, pool, (IDirect3DVertexBuffer9 **)vb->GetVertBuff(), NULL))) {
				delete vb;
				*ppVB = NULL;
				return IC_FAIL_CREATE_VB;
			}
			memcpy(vb->GetVertDef(), pVertDef, sizeof(vert_def));
			*ppVB = vb;
			return IC_OK;
		} else
			return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION CreateVB( class icVertBuffer** ppVB )

/*! Releases vertex buffer memory back to graphics card
 *
 *    @param[in]    pVB            Pointer to VB to release
 *    @returns    ICRESULT    Result of releasing VB
 *
 **/
ice::result gx_device_dx9::ReleaseVB(class vert_buffer *pVB) {
	delete pVB;
	return IC_OK;
} // END FUNCTION ReleaseVB( class icVertBuffer* pVB )

/*! Creates a dx9 index buffer
 *
 *    @param[out]    ppIB        Pointer to ouput IB pointer
 *    @param[in]    pIndexDef    Information about the IB
 *    @returns    ICRESULT    Result of creating IB
 *
 **/
ice::result gx_device_dx9::CreateIB(class index_buf **ppIB, struct index_def *pIndexDef) {
	assert(ppIB && pIndexDef);
	if (m_pDevice) {
		index_buf_dx9 *ib = new index_buf_dx9();
		if (ib) {
			ice::uchar size   = 2;
			D3DFORMAT  format = D3DFMT_INDEX16;
			if (pIndexDef->indexSize == ic_index_size_32) {
				size   = 4;
				format = D3DFMT_INDEX32;
			}
			ice::uint length = pIndexDef->numIndex * size;
			DWORD     usage  = D3DUSAGE_WRITEONLY;
			D3DPOOL   pool   = D3DPOOL_DEFAULT;
			if (FAILED(m_pDevice->CreateIndexBuffer(length, usage, format, pool,
			                                        (IDirect3DIndexBuffer9 **)ib->GetIndexBuffer(),
			                                        NULL))) {
				delete ib;
				*ppIB = NULL;
				return IC_FAIL_CREATE_IB;
			}
			memcpy(ib->GetIndexDef(), pIndexDef, sizeof(index_def));
			*ppIB = ib;
			return IC_OK;
		} else
			return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION CreateIB( icIndexBuf**, icIndexDef* )

/*! Releases index buffer from memory
 *
 *    @param[in]    pIB            Pointer to index buffer
 *    @returns    ICRESULT    Result of index buffer release
 *
 **/
ice::result gx_device_dx9::ReleaseIB(class index_buf *pVB) {
	delete pVB;
	return IC_OK;
} // END FUNCTION ReleaseIB( class icIndexBuf* pIB )

/*! Creates a dx9 texture
 *
 *    @param[out]    ppTex        Pointer to ouput texture pointer
 *    @param[in]    pTexDef        Information about the texture
 *    @returns    ICRESULT    Result of creating texture
 *
 **/
ice::result gx_device_dx9::CreateTex(class texture **ppTex, struct tex_def *pTexDef) {
	assert(ppTex && pTexDef);
	if (m_pDevice) {
		texture_dx9 *tex = new texture_dx9();
		if (tex) {
			DWORD     usage  = (pTexDef->usage) ? D3DUSAGE_DYNAMIC : 0;
			D3DFORMAT format = D3DFMT_A8R8G8B8;
			D3DPOOL   pool   = D3DPOOL_DEFAULT;
			if (FAILED(m_pDevice->CreateTexture(pTexDef->width, pTexDef->height, pTexDef->mipLevels,
			                                    usage, format, pool, (DX9TEX **)tex->GetTexture(),
			                                    NULL))) {
				delete tex;
				*ppTex = NULL;
				return IC_FAIL_CREATE_TEX;
			}
			memcpy(tex->GetTexDef(), pTexDef, sizeof(tex_def));
			*ppTex = tex;
			return IC_OK;
		} else
			return IC_FAIL_OUT_OF_MEM;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION CreateTex( class icTextue**, struct icTexDef )

/*! Creates an icTexture from a file
 *
 *    @param[in/out]    ppTex        Pointer to ouput texture pointer
 *    @param[in]        szFile        Name of file to load as texture
 *    @returns        ICRESULT    Result of texture creation
 *
 **/
ice::result gx_device_dx9::CreateTexFromFile(class texture **ppTex, const char *szFile) {
	assert(ppTex && szFile);
	if (m_pDevice) {
		ice::util::file_name file;
		file.ParseFileName(szFile);
		const char *ext = file.GetFileExtension();
		if (ext) {
			// Direct x supports these files
			if (!strcmp(ext, ".bmp") || !strcmp(ext, ".dds") || !strcmp(ext, ".dib") ||
			    !strcmp(ext, ".hdr") || !strcmp(ext, ".jpg") || !strcmp(ext, ".pfm") ||
			    !strcmp(ext, ".png") || !strcmp(ext, ".ppm") || !strcmp(ext, ".tga")) {
				texture_dx9 *tex = new texture_dx9();
				if (tex) {
					if (FAILED(
					        (*m_CreateTexFunc)(m_pDevice, szFile, (DX9TEX **)tex->GetTexture()))) {
						delete tex;
						return IC_FAIL_CREATE_TEX;
					}
					*ppTex = tex;
					return IC_OK;
				} else
					return IC_FAIL_OUT_OF_MEM;
			}
			return IC_FAIL_FILE_NOT_SUPPORTED;
		}
		return IC_FAIL_FILE_NOT_SUPPORTED;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION CreateTexFromFile( class icTexutre** ppTex )

/*! Releases a texture from graphics memory
 *
 *    @param[in]    pTex        Pointer to texture to release
 *    returns        ICRESULT    Result of texture release
 *
 **/
ice::result gx_device_dx9::ReleaseTex(class texture *pTex) {
	delete pTex;
	return IC_OK;
} // END FUNCTION ReleaseTex( class icTexture* pTex )

ice::result gx_device_dx9::CreateFont(class font **ppIFont, struct font_params *pFontParams) {
	return IC_OK;
}

ice::result gx_device_dx9::ReleaseFont(class font *ppIFont) { return IC_OK; }

ice::result gx_device_dx9::EnableAlphaBlend(bool bAlphaBlend) {
	if (m_pDevice) {
		m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

ice::result gx_device_dx9::EnableDepthTest(bool bDepth) {
	if (m_pDevice) {
		if (bDepth) {
			m_pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			// m_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		} else {
			m_pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			// m_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		}
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

ice::result gx_device_dx9::EnableFog(bool bEnable, const fog_params &params) { return IC_OK; }

/*! Sets the ambient light color
 *
 *    @param        col            Ambient light color
 *    @returns    ICRESULT    Result of state change
 *
 **/
ice::result gx_device_dx9::SetAmbientColor(const ice::math::color &col) {
	// ID3DXEffect* ef;
	// ef->Set
	return IC_OK;
} // END FUNCTION SetAmbientColor( const icColor& col )

ice::result gx_device_dx9::SetClearColor(const ice::math::color &col) {
	m_colClear = col;
	return IC_OK;
}

/*! Sets the device triangle cull mode
 *
 *    @param[in]    cMode_e        Tells the winding order for culling
 *    @returns    ICRESULT    Result of cull mode change request
 *
 **/
ice::result gx_device_dx9::SetCullMode(ice::render::IC_CULL_MODE cMode_e) {
	if (m_pDevice) {
		m_pDevice->SetRenderState(D3DRS_CULLMODE, (D3DCULL)(cMode_e + 1));
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION SetCullMode( IC_CULL_MODE cMode_e )

/*! Sets the projection matrix
 *
 *    @param        proj        The current projection matrix
 *    @returns    ICRESULT    Result of the state change
 *
 **/
ice::result gx_device_dx9::SetProjection(const ice::math::mat4 &proj) {
	if (m_pDevice) {
		m_pDevice->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX *)(const ice::math::real *)proj);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION SetProjection( const icMatrix44& proj )

/*! Sets the view matrix
 *
 *    @param        view        The current view matrix
 *    @returns    ICRESULT    Result of the state change
 *
 **/
ice::result gx_device_dx9::SetViewMatrix(const ice::math::mat4 &view) {
	if (m_pDevice) {
		m_pDevice->SetTransform(D3DTS_VIEW, (const D3DMATRIX *)(const ice::math::real *)view);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION SetViewMatrix( const icMatrix44& view )

/*! Set the world transform matrix
 *
 *    @param        world        The current world transform
 *    @returns    ICRESULT    Result of the state change
 *
 **/
ice::result gx_device_dx9::SetWorldTransform(const ice::math::mat4 &world) {
	if (m_pDevice) {
		m_pDevice->SetTransform(D3DTS_WORLD, (const D3DMATRIX *)(const ice::math::real *)world);
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
} // END FUNCTION SetWorldTransform( const icTransform& world )

ice::result gx_device_dx9::SetTexture(ice::uint index, texture *pTex) {
	if (!pTex) {
		return IC_WARN_GEN;
	}
	if (m_pDevice) {
		m_pDevice->SetTexture(index, ((texture_dx9 *)pTex)->GetTex());
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

/*! This may be un-nessecary
 *
 **/
void gx_device_dx9::ChangeSize(ice::ushort sizeX, ice::ushort sizeY) {
} // END FUNCTION ChangeSize( ushort sizeX, ushort sizeY )

#endif // #if (defined(_WIN32) && defined(ICDX9))

} // namespace ice::render
