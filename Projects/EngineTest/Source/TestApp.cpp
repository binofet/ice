
#include "TestApp.h"
#include "Util/icFileName.h"



TestApp::TestApp( void )
{
	_font = NULL;
	m_bRunning = true;
	
}

TestApp::~TestApp( void )
{
}

ICRESULT TestApp::Init(int i32ArgC, char* pszArgs[] )
{
	ICRESULT res = IC_OK;

	res = icApplication::Init( i32ArgC, pszArgs );
	if (ICEFAIL(res)) return res;

	res = _delegatesTest.Init();
	if (ICEFAIL(res)) return res;

	_routeBuffer = new icRouteBuffer();

	return res;
}

ICRESULT TestApp::Cleanup( void )
{
	return icApplication::Cleanup();
}

void TestApp::LookAt( const icVector3& eye, const icVector3& lookAt, const icVector3& up )
{
	m_Camera.SetCameraPos(eye);

	// compute view vectors
	icVector3 view = lookAt - eye;
	icVector3 right;
	icVector3 viewUp;
	view.Normalize();
	right =  Cross(view, up );
	right.Normalize();
	viewUp =  Cross(right, view );
	viewUp.Normalize();

	// now set up matrices
	// base rotation matrix
	icMatrix33 rotate;
	rotate.SetColumns( right, viewUp, -view );

	// world->view transform
	// set rotation
	rotate.Transpose();
	icVector3 invEye = -(rotate*eye);
	icMatrix44 mWorldToViewMatrix(rotate, invEye);

	// send to device
	m_pGxDevice->SetViewMatrix( mWorldToViewMatrix );
}

ICRESULT TestApp::Update(void)
{
#ifndef __APPLE__
	_delegatesTest.UpdateTest();
#endif
	if (m_pGxDevice)
	{
		float deltaTime = _timer.Elapsed();
		_maxCam.Update(deltaTime);
	}
	return icApplication::Update();
}

ICRESULT TestApp::Render( void )
{
	if (_pathingSystem)
	{
		icTimer timer;

		timer.Elapsed();
		_pathingSystem->TryPlotRoute(icVector3(0.0f,0.0f,0.0f), icVector3(99.0f, 49.0f, 0.0f), *_routeBuffer);

		float routeTime = timer.Elapsed();
		icPrintf("route time = %lf", routeTime);
	}

	if (!m_pGxDevice)
		return IC_FAIL_GEN;

	m_pGxDevice->FrameBegin();

	m_pGxDevice->SetProjection(_maxCam.GetProjection());
	m_pGxDevice->SetViewMatrix(_maxCam.GetViewMat());

	m_pGxDevice->SetCullMode(IC_CULL_CLOCKWISE);
	icMatrix44 cubeMatrix = icMatrix44::IDENTITY;
	if (_defaultCubeModel)
	{
		_defaultCubeModel->Render(cubeMatrix);
	}

	//m_pGxDevice->SetCullMode(IC_CULL_ANTICLOCK);
	if (_font)
	{
		_font->Printf(icVector2Int(10,20), icColor::BLACK, "UFPS: %.2f", m_pGxDevice->UFPS());
		_font->Printf(icVector2Int(10,10), icColor::BLACK, "RFPS: %.2f", m_pGxDevice->RFPS());
	}

	m_pGxDevice->FrameEnd();

	return IC_OK;
}
