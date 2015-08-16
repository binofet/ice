#ifndef __IC_FILE_H__
#define __IC_FILE_H__


#include "Core/icGlobals.h"


enum ICFILEMODE
{
    ICFM_INVALID            =0,

    ICFM_READ               =(1<<0), //! Read file rights
    ICFM_WRITE              =(1<<1), //! Write file rights

    ICFM_CREATE_ALWAYS      =(1<<2), //! Creates file, always.
    ICFM_CREATE_NEW         =(1<<3), //! Creates file, if not already exist.

    ICFM_OPEN_ALWAYS        =(1<<4), //! Opens a file, always.
    ICFM_OPEN_EXISTING      =(1<<5), //! Opens a file, only if it exists.

    ICFM_ASYNC              =(1<<6)  //! can ONLY use Asynchronous calls
};


#define ICFMREAD_CREATE         (ICFM_READ | ICFM_OPEN_ALWAYS)
#define ICFMREAD_EXISTING       (ICFM_READ | ICFM_OPEN_EXISTING)
#define ICFMREAD_NEW            (ICFM_READ | ICFM_OPEN_ALWAYS)

#define ICFMWRITE_CREATE        (ICFM_WRITE | ICFM_OPEN_ALWAYS)
#define ICFMWRITE_EXISTING      (ICFM_WRITE | ICFM_OPEN_EXISTING)
#define ICFMWRITE_NEW           (ICFM_WRITE | ICFM_CREATE_ALWAYS)

#define ICFMREAD_WRITE_NEW      (ICFM_READ | ICFM_WRITE | ICFM_CREATE_ALWAYS)
#define ICFMREAD_WRITE_CREATE   (ICFM_READ | ICFM_WRITE | ICFM_OPEN_ALWAYS)
#define ICFMREAD_WRITE_EXIST    (ICFM_READ | ICFM_WRITE | ICFM_OPEN_EXISTING)


/*! Class provides file i/o
**/
#if defined(WIN32)
class icFile : private OVERLAPPED
#else
class icFile
#endif
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icFile(void);
    ~icFile(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT Open(const char* szFile, uchar u8Mode);
    ICRESULT Close(void);

    ICRESULT Read(void* pDest, size_t size, size_t* sizeread);
    ICRESULT ReadAsync(void* pDest, size_t size,
                       void* userPtr, void (*callback)(void*,size_t));

    ICRESULT Write(void* pSource, size_t size, size_t* sizewritten);
    ICRESULT WriteAsync(void* pSource, size_t size,
                        void* userPtr, void (*callback)(void*,size_t));

    ICRESULT StopAsync(void);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    ICRESULT GetSize(uint64* size);
    ICRESULT GetPos(uint64* pos);

    ICRESULT SetPos(const uint64 pos);

private:
#if defined(WIN32)
    static VOID CALLBACK AsyncCB(DWORD dwErrorCode,
                                 DWORD dwBytesMoved,
                                 LPOVERLAPPED lpOverlapped);

    HANDLE          m_pFile;           //! File Handle
#else
    FILE*           m_fp;
#endif

    uint64          m_u64FilePos;      //! Current File Position

    volatile bool   m_bStreaming;      //! Asynchronous operation taking place
    uchar           m_FileMode;        //! File mode, attributes, creation

    void*           m_ptrUser;
    void (*m_pVoidCallback)(void*,size_t);
};

#endif //__IC_FILE_H__