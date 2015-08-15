
#ifdef WIN32

#include "Core/IO/icFile.h"


/*! Function converts ICE file modes to Microsoft flags
 *
 *  @param      u8Mode      8-bit mask of ICFILEMODE flags
 *  @param[out] pAccess     Pointer for access rights
 *  @param[out] pCreateD    Pointer for creation disposition
 *  @param[out] pFandA      Pointer for flags and attributes
**/
void _GetFileParams(uchar u8Mode, DWORD* pAccess, DWORD* pCreateD,
                    DWORD* pFandA)
{
    *pAccess=0; *pFandA = 0;

    if (u8Mode&ICFM_READ) *pAccess |= GENERIC_READ;
    if (u8Mode&ICFM_WRITE) *pAccess |= GENERIC_WRITE;

    if (u8Mode&ICFM_CREATE_ALWAYS) *pCreateD = CREATE_ALWAYS;
    if (u8Mode&ICFM_CREATE_NEW) *pCreateD = CREATE_NEW;
    if (u8Mode&ICFM_OPEN_ALWAYS) *pCreateD = OPEN_ALWAYS;
    if (u8Mode&ICFM_OPEN_EXISTING) *pCreateD = OPEN_EXISTING;

    if (u8Mode&ICFM_ASYNC) *pFandA |= FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING;
}// END FUNCTION _GetFileParams(uchar u8Mode, DWORD* pAccess, DWORD* pCreateD)


/*! Asynchronous operation callback
 *
 *
 *  @param      dwErrorCode     Error flags
 *  @param      dwBytesMoved    The number of bytes transferred
 *  @param      lpOverlapped    The overlapped structure for async op
**/
VOID CALLBACK icFile::AsyncCB(__in  DWORD dwErrorCode,
                              __in  DWORD dwBytesMoved,
                              __in  LPOVERLAPPED lpOverlapped)
{
    if (lpOverlapped)
    {
        icFile* pFile = static_cast<icFile*>(lpOverlapped);

        // check for errors
        if (dwErrorCode)
        {
            // should probably take a look at better way to handle this
            icWarning("There was an error returned from Async Operation");
        }

        pFile->m_u64FilePos += dwBytesMoved;
        pFile->m_bStreaming = false;

        // call user callback function with their pointer
        if (pFile->m_pVoidCallback)
            (*pFile->m_pVoidCallback)(pFile->m_ptrUser, (size_t)dwBytesMoved);
    }
    else
    {
        // this should likely never happen
        icError("Undefined behavior in Asynchronous callback");
    }
}


/*! c'tor
**/
icFile::icFile(void)
{
    ZeroMemory(this,sizeof(icFile));

    m_pFile = NULL;
    m_ptrUser = NULL;
    m_pVoidCallback = NULL;

    m_u64FilePos = 0;

    m_bStreaming = false;
}// END FUNCTION icFile(void)


/*! d'tor
**/
icFile::~icFile(void)
{
    if (m_pFile)
    {
        StopAsync();

        if (hEvent)
            CloseHandle(hEvent);

        CloseHandle(m_pFile);
    }
}// END FUNCTION ~icFile(void)


/*! Opens a file
 *
 *  @param      szFile      Name of file to open
 *  @param      u8Mode      File mode (read/write/etc)
 *  @returns    ICRESULT    Status after open
**/
ICRESULT icFile::Open(const char *szFile, uchar u8Mode)
{
    Close();

    DWORD dwAccess=0;
    DWORD dwShareMode=FILE_SHARE_READ; //! should this be exposed?
    DWORD dwCreateDisp=0;
    DWORD dwFandA=0;

    m_FileMode = u8Mode;
    _GetFileParams(u8Mode, &dwAccess, &dwCreateDisp, &dwFandA);

    m_pFile = CreateFileA(szFile,           // LPCSTR
                          dwAccess,         // Desired Access
                          dwShareMode,      // Share Mode
                          NULL,             // lpSecurityAttributes
                          dwCreateDisp,     // Creation Disposition
                          dwFandA,          // Flags and attributes
                          NULL);            // HANDLE to template file
   
    if (m_pFile &&  m_pFile != INVALID_HANDLE_VALUE)
        return IC_OK;
    return IC_FAIL_GEN;
}// END FUNCTION Open(const char* szFile, u


/*! Closes the file
 *
 *      This will stall in the event there is an asynchronous operation
 *      still in progress.
 *
 *  @returns    ICRESULT        Status after closing the file
**/
ICRESULT icFile::Close(void)
{
    m_FileMode = 0;
    m_ptrUser = NULL;
    m_pVoidCallback = NULL;
    m_u64FilePos = 0;

    if (m_pFile)
    {
        // WAIT FOR ANY PENDING ASYNCHRONOUS CALLS
        while (m_bStreaming)
            SleepEx(50, TRUE);

        if (hEvent)
        {
            CloseHandle(hEvent);
            hEvent = NULL;
        }

        if (CloseHandle(m_pFile))
        {
            m_pFile = NULL;
            return IC_OK;
        }
        m_pFile = NULL;
        return IC_FAIL_GEN;
    }

    m_bStreaming = false;

    return IC_OK;
}// END FUNCTION Close(void)


/*! Reads data from file
 *
 *      Note: This should not be called by ASYNC File objects
 *
 *  @param      pDest       Destination buffer
 *  @param      size        Size in bytes to read
 *  @param      sizeread    Pointer to store size actually read
 *  @returns    ICRESULT    Status after file read
**/
ICRESULT icFile::Read(void* pDest, size_t size, size_t* sizeread)
{
    if (m_pFile && !(m_FileMode&ICFM_ASYNC))
    {
        if (ReadFile(m_pFile, pDest, size, (LPDWORD)sizeread, NULL))
        {
            m_u64FilePos += *sizeread;
            return IC_OK;
        }
    }
    return IC_WARN_GEN;
}// END FUNCTION Read(void* pDest, size_t size, size_t* sizeread)


/*! Asynchronous Read
 *
 *
 *
 *  @param      pDest       Destination buffer
 *  @param      size        Size in bytes to read
 *  @param      userPtr     Pointer user can use as needed
 *  @param      callback    Function to call when read is finished
 *  @returns    ICRESULT    Status after starting the async-read
**/
ICRESULT icFile::ReadAsync(void* pDest, size_t size, 
                           void* userPtr, void (*callback)(void*,size_t))
{
    if (m_pFile && !m_bStreaming)
    {
        m_pVoidCallback = callback;
        m_ptrUser = userPtr;
        m_bStreaming = true;
        hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
        if (ReadFileEx(m_pFile, pDest, size, 
                       (LPOVERLAPPED)this, AsyncCB))
            return IC_OK;
        m_bStreaming = false;
    }
    return IC_WARN_GEN;
}// END FUNCTION ReadAsync(void* pDest, size_t size, void (*callback)(void))


/*! Stops all Asynchronous operations
 *
 *  @returns    ICRESULT        Status after stopping asynchronous ops
**/
ICRESULT icFile::StopAsync(void)
{
    if (m_pFile && m_bStreaming)
    {
        if (CancelIo(m_pFile))
        //if (CancelIoEx(m_pFile, this))
        {
            m_bStreaming = false;
            return IC_OK;
        }
    }
    return IC_WARN_GEN;
}// END FUNCTION StopRead(void)


/*! Writes data to a file
 *
 *  @param      pSource     Pointer to data to be written to file
 *  @param      size        Size of data (in bytes) to write
 *  @param[out] sizewritten Size of data actually written to the file
 *  @returns    ICRESULT    Status after the file write
**/
ICRESULT icFile::Write(void* pSource, size_t size, size_t* sizewritten)
{
    if (m_pFile && !(m_FileMode&ICFM_ASYNC))
    {
        if (WriteFile(m_pFile, pSource, (DWORD)size, 
                      (LPDWORD)sizewritten, NULL))
        {
            m_u64FilePos += *sizewritten;
            return IC_OK;
        }
    }
    return IC_FAIL_GEN;
}// END FUNCTION Write(void* pSource, size_t size, size_t* sizewritten)


/*! Writes to a file asynchronously
 *
 *  @param      pSource     Pointer to data to be written to file
 *  @param      size        Size of data (in bytes) to write
 *  @param      userPtr     Pointer for user to use in callback
 *  @param      callback    Function pointer for user callback
 *  @returns    ICRESULT    Status after the file write
**/
ICRESULT icFile::WriteAsync(void* pSource, size_t size,
                            void* userPtr, void (*callback)(void*,size_t))
{
    if (m_pFile && !m_bStreaming)
    {
        m_pVoidCallback = callback;
        m_ptrUser = userPtr;
        m_bStreaming = true;
        hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
        if (WriteFileEx(m_pFile, pSource, (DWORD)size, (LPOVERLAPPED)this, AsyncCB))
        {
            FlushFileBuffers(m_pFile);
            return IC_OK;
        }
        else
        {
            m_bStreaming = false;
            DWORD err = GetLastError();
            icWarningf("icFile::WriteAsync failed with error: %i",err);
        }
    }
    return IC_FAIL_GEN;
}// END FUNCTION WriteAsync(void* pSource, size_t size,
 //                         void* userPtr, void (*callback)(void*,size_t))


/*! Get File Size in bytes
 *
 *  @param      size        Pointer to store file size
 *  @returns    ICRESULT    Status after getting size
**/
ICRESULT icFile::GetSize(uint64* size)
{
    if (m_pFile)
    {
        DWORD high=0;
        DWORD low = GetFileSize(m_pFile, &high);

        // check fail condition
        if (low != INVALID_FILE_SIZE)
        {
            *size = (uint64)low | (uint64)high<<32;
            return IC_OK;
        }
    }
    return IC_FAIL_GEN;
}// END FUNCTION GetSize(size_t* size)


/*! Get the current file position
 *
 *
 *  @param      pos         Pointer to store file position
 *  @returns    ICRESULT    Status after getting file position
**/
ICRESULT icFile::GetPos(uint64* pos)
{
    if (m_pFile)
    {
        *pos = m_u64FilePos;
        return IC_OK;
    }
    return IC_FAIL_GEN;
}// END FUNCTION GetPos(size_t* pos)


/*! Sets the file pointer
 *
 *  @param      pos         Desired file position
 *  @returns    ICRESULT    Status after changing file position
**/
ICRESULT icFile::SetPos(const uint64 pos)
{
    if (m_pFile)
    {
#if 1
        LARGE_INTEGER liPos;
        liPos.QuadPart = pos;
        if (SetFilePointerEx(m_pFile, liPos, 
                             (PLARGE_INTEGER)&m_u64FilePos, FILE_BEGIN))
#else
        long liPos = (long)pos;
        if (SetFilePointer(m_pFile, liPos, 
                             0, FILE_BEGIN))
#endif
            return IC_OK;
    }
    ::MessageBoxA(NULL, "Failed to set file position", "Shit Ballz", 0);
    return IC_FAIL_GEN;
}// END FUNCTION SetPos(const uint64 pos)

#endif// ifdef WIN32

