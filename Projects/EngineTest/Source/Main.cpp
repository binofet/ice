#include "TestApp.h"


#define USE_OPENAL_AUDIO
//#define USE_XAUDIO2

#define USE_OPENGL_RENDER
//#define USE_OPENGLES_RENDER
//#define USE_DX9_RENDER
//#define USE_DX11_render


#include "Audio/icSound.h"
#if defined(USE_OPENAL_AUDIO)
#include "Audio/icSoundAL.h"
#endif

#include "Core/Application/icApplication.h"
#include "Core/Application/Window/icWindow.h"
#include "Core/Threading/icThread.h"

#if defined(USE_OPENGL_RENDER)
#include "Core/GXDevice/icGXDeviceGL.h"
#elif defined(USE_DX9_RENDER)
#include "Core/GXDevice/icGXDeviceDX9.h"
#else
#error
#endif

#include "Memory/icMemory.h"
#include "Memory/AllocationDebug.h"
#include "Math/icRand.h"
#include "ObjectModel/icObject.h"
#include "ObjectModel/icType.h"
#include "Scripting/IScriptDomain.h"
#include "Scripting/Debugging/IDebugger.h"
#include "Scripting/Debugging/LuaDebugger.h"
#include "Scripting/LuaScriptDomain.h"
#include "Util/icTime.h"

//#include "NuiCore/icNuiCore.h"

//using namespace Ice::Core::Debug;
using namespace Ice::ObjectModel::Reflection;

using namespace Ice::Scripting;
using namespace Ice::Scripting::Lua;
using namespace Ice::Scripting::Debugger;
using namespace Ice::Scripting::Debugger::Lua;

//using namespace Ice::Nui;


enum Target
{
	_x86,
	_x64,
	_ppc,
	_arm
};

enum Platform
{
	_win32,
	_win64,
	_osx,
	_x11_32,
	_x11_64,
	_ios3,
	_ios4,
	_ios5
};

enum IceRenderer
{
	_OpenGL,
	_OpenGLES,
	_DirectX9,
	_DirectX11
};

TestApp app;
IScriptDomain* gScript = 0;
IDebugger* gDbg = 0;

#define MULTITHREADED

/*! To test threading
**/
ICRESULT TestVoid(void* data)
{
	(void)data;
	#define COUNT 1024
	int rt[COUNT];
	float rf[COUNT];
	double rd[COUNT];
	icReal rr[COUNT];
	icTimer timer;
	for (int i=0; i<COUNT; ++i)
	{
		rt[i] = icRandUI(2,7);

		char* mem = new char[rt[i] * 100];

		rf[i] = icRandF(50.0f, 51.0f);
		rd[i] = icRandD(100.0, 105.0);
		rr[i] = icRandR();
		icPrintf("%i, %i, %f, %f", i ,rt[i], rf[i], timer.Elapsed());
		icThread::Sleep(100);

		delete[] mem;

		if (!app.IsRunning())
			return 0;
	}
	return 0;
}// END FUNCTION TestVoid(void* data)



#if !defined(_XBOX) && (defined(WIN32) || defined(_WIN32))


#include <windows.h>

HINSTANCE g_hInstance;
HWND g_hwnd;
int _stdcall wWinMain(HINSTANCE hIntance, HINSTANCE hPrevInstance,
	LPWSTR args, int argc)
	//int _stdcall WinMain(HINSTANCE hIntance, HINSTANCE hPrevInstance,
	//                     LPSTR args, int argc)
{(void)hPrevInstance;
g_hInstance = hIntance;
//uint winFlags2 = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
uint winFlags = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

#elif defined (_XBOX)
    
    
#include <xbox.h>

void __cdecl main()
{
	uint winFlags = 0;
    
    
#elif defined __APPLE__
    
uint winFlags = kWindowCloseBoxAttribute |
	kWindowCollapseBoxAttribute |
	kWindowStandardHandlerAttribute ;
//uint winFlags = kWindowNoTitleBarAttribute;
    

    
int main(int argc, char* args[])
{
#else
    
	uint winFlags = 0;
    
#endif

	long ICE_STATUS = IC_OK;

	// initialization
	icMemory::Init();
	icSysTimeInit();
	icRandAutoSeed();

	LuaDebugger* debugger = new LuaDebugger();
	gDbg = debugger;
	LuaScriptDomain* lua = new LuaScriptDomain(gDbg);
	gScript = lua;

	char testBuffer[] = "print(\"Hello from \".._VERSION)";
	ICE_STATUS = lua->ExecuteBuffer(testBuffer, sizeof(testBuffer));

	if (ICEFAIL(ICE_STATUS))
	{
		icPrintf("failed to run lua test script");
	}

	icMemoryPoolI* pool = icMemory::getStringPool();
	icMemoryPoolI* pool2 = icMemory::getTlsfPool();

	icTimer timer;
	timer.Elapsed();

	for (int i = 0; i < 10000; ++i)
	{
		int len = icRandI(8, 100);
		char* str = (char*)malloc(len);
		if (len % 2 == 0)
		{
			free(str);
		}
	}
	icReal stringTime = timer.Elapsed();
	icPrintf("Windows malloc: - %lf", stringTime);


	timer.Elapsed();
	for (int i = 0; i < 10000; ++i)
	{
		int len = icRandI(8, 100);
		char* str = (char*)pool->malloc(len);
		if (len % 2 == 0)
		{
			pool->free(str);
		}
	}
	stringTime = timer.Elapsed();
	icPrintf("dlmalloc: - %lf", stringTime);

	timer.Elapsed();
	for (int i = 0; i < 10000; ++i)
	{
		int len = icRandI(8, 100);
		char* str = (char*)pool2->malloc(len);
		if (len % 2 == 0)
		{
			pool2->free(str);
		}
	}
	stringTime = timer.Elapsed();
	icPrintf("tlsf: - %lf", stringTime);

	timer.Elapsed();
	icThread* curThread = icThread::GetCurrent();
	stringTime = timer.Elapsed();
	icPrintf("GetCurrentThread - %lf", stringTime);

#if defined(USE_OPENAL_AUDIO)
	icSoundDeviceAL m_Sound;
#elif defined(USE_DX8_AUDIO)
	icSoundDeviceDX8 m_Sound;
#endif

	Object* pObject = new Object();
	//NuiCore* nuiCore = new NuiCore();

	const Type* const objType = pObject->GetType();
	//const Type* const nuiType = nuiCore->GetType();

	Object* newObject = objType->Create();

	Object* foo = Object::ClassType()->Create();

	//icPrintf("foo = ", foo->ToString());

	Object* blah = Type::GetType("Ice.Object")->Create();

	m_Sound.Initialize();

	icSoundParam params;
	params.bLoop = true;
	params.bPaused = false;
	params.fPitch = 1.0f;
	params.fVol = 1.0f;

	icSoundI* sound;
	if (ICEFAIL(m_Sound.Play("original.wav",params,&sound)))
	{
		icPrint("Failed to play sound: original.wav");
	}

	icWindowDef window = {winFlags,640,480,100,100,"Engine Test",false};
	icWindow* pWindow = icWindow::GetWindow(window);

	//icWindowDef window2 = {winFlags2,400,400,600,100,"Engine Test2",false};
	//icWindow* pWindow2 = icWindow::GetWindow(window2, pWindow);

	// Startup the app;
	icPrintf("Initializing EngineTest . . .",NULL);
	ICE_STATUS = app.Init( 0, NULL );
	if (ICEFAIL(ICE_STATUS))
#if defined (_XBOX)
		return;
#else
		return ICE_STATUS;
#endif
	icPrintf("Initializing App Done. Status: %s", icGetStr(ICE_STATUS));

#if defined(USE_OPENGL_RENDER)
	icGXDevice* pDev = new icGXDeviceGL();
#elif defined(USE_DX9_RENDER)
	icGXDevice* pDev = new icGXDeviceDX9();
#elif defined(USE_DX8_RENDER)
	icGXDevice* pDev = new icGXDeviceDX8();
#else
#error
#endif

#if defined(USE_OPENAL_AUDIO)
	icSoundDeviceI* pSoundDevice = new icSoundDeviceAL();
#elif defined(USE_DX8_AUDIO)
	icSoundDeviceI* pSoundDevice = new icSoundDeviceDX8();
#endif

	pDev->Init(pWindow);

	app.SetEngine(pDev);
	app.SetWindow(pWindow);

	pWindow->Show();
	//pWindow2->Show();

#ifdef MULTITHREADED
	icThread *pv,*pThread,*pThread2;

	//icThreadCreate(&TestVoid,(void*)0, &pv);
	icThreadCreate<TestApp>(&app, &TestApp::UpdateThread, 0, &pThread);
	icThreadCreate<TestApp>(&app, &TestApp::RenderThread, 0, &pThread2);
#endif

	// Message loop
#ifdef WIN32
	MSG msg;
	ZeroMemory(&msg,sizeof(MSG));
	while (msg.message != WM_QUIT && ICE_STATUS >= IC_OK)
	{
		lua->ExecuteBuffer("print \"hello world!\"", sizeof("print \"hello world!\""));
		lua->ExecuteFile("Resource/scripts/test.lua");
		lua->Update(0.016f);
		gDbg->Update(0.15f);
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

#ifndef MULTITHREADED
		ICE_STATUS = app.Update();
		ICE_STATUS |= app.Render();
#endif
	}

	// let the threads know its over
	app.ToggleRunning(false);

	icPrintf("Main loop exited with status: %s", icGetStr(ICE_STATUS));


#ifdef MULTITHREADED
	// wait for threads to exit
	volatile bool isActive;;
	do
	{
		isActive = pv->IsThreadActive() ||
			pThread->IsThreadActive() ||
			pThread2->IsThreadActive();
	}while (isActive);
#endif

	if (lua)
	{
		delete lua;
		lua = 0;
	}

	ICE_STATUS = app.Cleanup();

	//AllocationDebug::SaveToFile("C:\\temp\\mem_dump.txt");

	icMemory::Shutdown();

	icPrintf("App Cleanup result: %s", icGetStr(ICE_STATUS));

#if !defined (_XBOX)
	return (int)ICE_STATUS;
#endif
};
