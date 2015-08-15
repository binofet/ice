#ifndef __TEST_APP_H__
#define __TEST_APP_H__

#include "AI/Pathing/icPathingSystemI.h"
#include "Audio/icSound.h"

#include "Core/Application/icApplication.h"
#include "Core/Events/icEvent.h"
#include "Math/Vector/icVector3.h"
#include "Render/Texture/icTexture.h"
#include "Render/VertBuffer/icVertBuffer.h"
#include "Render/IndexBuffer/icIndexBuf.h"
#include "Render/Camera/icCamera.h"
#include "Render/Camera/icMaxCam.h"
#include "Render/icModel.h"
#include "Render/Font/icFont.h"

#include "Content/icContentLoader.h"

#include "Util/icTime.h"

#include "DelegateTests.h"

class TestApp : public icApplication
{
public:
	TestApp( void );
	~TestApp( void );

	ICRESULT Cleanup( void );
	ICRESULT Init(    int i32ArgC, char* pszArgs[] );

	//////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ICRESULT Update(void);
	ICRESULT Render( void );

	ICRESULT UpdateThread(void* pData)
	{(void)pData;
		ICRESULT stat = IC_OK;
		while(m_bRunning && !ICEFAIL(stat))
		{
			stat = Update();
		}
		if (m_bRunning)
		{
			icError("Update thread failed");
		}
		return stat;
	};

	ICRESULT RenderThread(void* pVoid)
	{(void)pVoid;
		m_pGxDevice->Init(m_Window);
		icFontParams fontParams = { "Arial", 15};
		m_pGxDevice->CreateFont(&_font, &fontParams);
		_contentLoader = new icContentLoader(m_pGxDevice, _soundDevice);
		_contentLoader->Load("Resource/models/iceCubeSimple.icm", &_defaultCubeModel);
		_contentLoader->Load("Resource/decent.path", &_pathingSystem);

		icWindowDef winDef;
		m_Window->GetParams(&winDef);

		icCreatePerspective(IC_PI/4.0f,(icReal)winDef.i16Width/(icReal)winDef.i16Height,1.0f,1000.0f,
			_maxCam.GetProjection());

		icVector3 cam_pos(0.0f,30.750f,-105.54f);
		icCreateLookAt(cam_pos,icVector3::ZERO,icVector3::Y_AXIS,m_Camera.GetViewMat());

		_maxCam.Zoom() = 67.000000f;
		_maxCam.ZoomMax() = 100.0f;
		_maxCam.ZoomMin() = 1.0f;
		_maxCam.Xrot() = 2.5525725f;
		_maxCam.Yrot() = 4.3513942f;
		_maxCam.Center().Set(0.f,0.f,0.f);

		ICRESULT stat = IC_OK;
		while(m_bRunning && !ICEFAIL(stat))
		{
			stat = Render();
		}
		if (m_bRunning)
		{
			icError("Render thread failed");
		}
		delete m_pGxDevice;
		return stat;
	};


	void LookAt( const icVector3& eye, const icVector3& lookAt, const icVector3& up );

	void SetEngine( icGXDevice* pEngine )
	{
		// This needs mutex in threaded mode!
		m_pGxDevice = pEngine;
	};

	void SetWindow( icWindow* pWin ) { m_Window = pWin; };

	void ToggleRunning( bool bRunning ) { m_bRunning = bRunning; };

	bool IsRunning() { return m_bRunning; };

private:
	icCamera        m_Camera;
	icCamera        m_Ortho;
	icMaxCam        _maxCam;

	volatile bool    m_bRunning;

	icWindow*        m_Window;

	int                m_test;

	icFontI*            _font;
	icContentLoader*    _contentLoader;
	icSoundDeviceI*     _soundDevice;
	icModel*            _defaultCubeModel;
	icPathingSystemI*   _pathingSystem;
	icRouteBuffer*      _routeBuffer;
	icTimer             _timer;

	DelegateTests       _delegatesTest;
};
#endif //__TEST_APP_H__