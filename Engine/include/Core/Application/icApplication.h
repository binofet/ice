#ifndef __IC_APPLICATION_H__
#define __IC_APPLICATION_H__


#include "Core/Application/Window/icWindow.h"
#include "Core/GXDevice/icGXDevice.h"



class icApplication
{
public:
    /////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icApplication(void);
    virtual ~icApplication(void);

    virtual ICRESULT Cleanup(void);

    virtual ICRESULT Init(int argc, char* args[]);

    /////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Update(void);
    virtual ICRESULT Render(void);
    virtual ICRESULT Quit(void);

    /////////////////////////////////////////////////////////////////////////
    // WINDOW CALLBACKS
    //virtual void OnChangeSize(const icWindow* const pWin,
    //    ushort sizex, ushort sizey){};

    //virtual void OnMinimize(const icWindow* const pWin){};

    //virtual void OnMaximize(const icWindow* const pWin){};

    //virtual void OnDrop(const icWindow* const pWin, void* pContext){};

protected:
    /////////////////////////////////////////////////////////////////////////
    // PROTECTED MEMBERS
    icGXDevice*     m_pGxDevice;

    int             m_iArgc;
    char**          m_pArgsz;
};

#endif //__IC_APPLICATION_H__

