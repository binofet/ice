
#ifdef __APPLE__

#include "Core/Application/Window/icWindowOSX.h"

OSStatus WindowProc(EventHandlerCallRef  nextHandler,
					EventRef             theEvent,
					void*                userData)
{
	OSStatus error;
	
	return error;
}



icWindowOSX::icWindowOSX(const icWindowDef& winDef) : icWindow(winDef, nullptr)
{
	m_pWindow = NULL;
	m_handlerUPP = NULL;
	Create();
}

icWindowOSX::~icWindowOSX(void)
{
	Cleanup();
}

ICRESULT icWindowOSX::Cleanup(void)
{
	if (m_pWindow)
	{
		//DisposeWindow(m_pWindow);
		m_pWindow = NULL;
	}
	return IC_OK;
}

void icWindowOSX::Create(void)
{
	Cleanup();
	
	Rect windowBounds;
	windowBounds.left = 0;
	windowBounds.top = 0;
	windowBounds.right = m_WindowInfo.i16Width;
	windowBounds.bottom = m_WindowInfo.i16Height;
	
	 
	OSStatus error;
	/*
	error = CreateNewWindow(kDocumentWindowClass,
							m_WindowInfo.ui32WinFlags,
							&windowBounds,
							&m_pWindow);
	*/
	assert(error == noErr && m_pWindow);
	
	
	m_handlerUPP = NewEventHandlerUPP( WindowProc );
	
	EventTypeSpec mainSpec[] = {kEventClassWindow,
								kEventClassKeyboard,
								kEventClassMouse,kEventClassAppleEvent,kEventClassMenu,
								kEventClassApplication,kEventClassAppearance};
	/*
	error = InstallWindowEventHandler(m_pWindow,
									  m_handlerUPP,
									  GetEventTypeCount(mainSpec),
									  mainSpec,
									  this,
									  NULL);
	*/
	assert(error == noErr && m_handlerUPP);
	/*
	SetWindowTitleWithCFString( m_pWindow, __CFStringMakeConstantString(m_WindowInfo.pWindowName));
	RepositionWindow( m_pWindow, NULL, kWindowCenterOnMainScreen );
    */
}


ICRESULT icWindowOSX::Show()
{
	//ShowWindow(m_pWindow);
	return IC_OK;
}

ICRESULT icWindowOSX::Hide()
{
	//HideWindow(m_pWindow);
	return IC_OK;
}

ICRESULT icWindowOSX::UpdateParams(const icWindowDef& winDef)
{
	return IC_OK;
}// END FUNCTION UpdateParams(const icWindowDef& winDef)
#endif // __APPLE__