
//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "airhockey.h"

//////////////////////////////////////////////////////////////////////////////
// ICE SYSTEM INCLUDES
#include "Core/Threading/icThread.h"
#include "Math/icRand.h"
#include "Memory/icMemory.h"
#include "Util/icTime.h"



//////////////////////////////////////////////////////////////////////////////
// GLOBALS
Airhockey app;
Airhockey& gApp(app);

#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)
#include <windows.h>

HINSTANCE g_hInstance;
HWND g_hwnd;
int _stdcall wWinMain(HINSTANCE hIntance, HINSTANCE hPrevInstance,
					  LPWSTR args, int argc)
{
	g_hInstance = hIntance;
#else
int main(int argc, char* args[])
{
#endif

	// ICE system initialization
	icMemory::Init();
	icSysTimeInit();
	icRandAutoSeed();


	ICRESULT ICE_STATUS = IC_OK;

	// Initialize the app
	if (ICEFAIL(app.Init(argc, (char**)args)))
	{
#ifdef WIN32
		MessageBoxA(NULL, "Airhockey can't start", "Error", 0);
#elif defined (__APPLE__)
#endif
		return -1;
	}

	// Message loop
#ifdef WIN32
	MSG msg;
	ZeroMemory(&msg,sizeof(MSG));
	while (msg.message != WM_QUIT && ICE_STATUS >= IC_OK)
	{
		if (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
#else
	volatile bool running = true;
	while (running)
	{
#ifdef __APPLE__
		EventRecord eventStruct;
		if (WaitNextEvent(everyEvent,&eventStruct,1,NULL))
		{
			if (eventStruct.what == kEventWindowClosed)
				running = false;
			if (eventStruct.what == kEventWindowClickCloseRgn)
				running = false;
		}
#endif
#endif

		ICE_STATUS = app.Update();
		ICE_STATUS |= app.Render();
	}

	icPrintf("Main loop exited with status: %s", icGetStr(ICE_STATUS));

	return ICE_STATUS;
};
