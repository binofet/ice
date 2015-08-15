#ifdef __APPLE__
#ifndef __IC_WINDOW_OSX_H__
#define __IC_WINDOW_OSX_H__

#include <Carbon/Carbon.h>
#include <AGL/agl.h>
#include <sched.h>
#include <pthread.h>
#include <sys/sysctl.h>

#include "Core/Application/Window/icWindow.h"

class icWindowOSX : public icWindow
{
public:
	icWindowOSX( const icWindowDef& winDef );
	virtual ~icWindowOSX( void );
	
	virtual ICRESULT Cleanup( void );
	
	/////////////////////////////////////////////////////
	// OPERATIONS
	virtual ICRESULT Show();
	virtual ICRESULT Hide();
	virtual ICRESULT UpdateParams( const icWindowDef& winDef );
	
    virtual ICRESULT GetWindowPos(icVector2Int* pV2Pos);
    
	FORCE_INLINE WindowRef GetRef(void) { return m_pWindow; };


    OSStatus WinProc(EventHandlerCallRef nextHandler,
                     EventRef			 theEvent);
	
private:
	void Create();
	
	WindowRef			m_pWindow;
	EventHandlerUPP		m_handlerUPP;
};

#endif //__IC_WINDOW_OSX_H__
#endif //__APPLE__