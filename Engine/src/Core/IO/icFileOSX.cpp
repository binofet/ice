
#ifdef __APPLE__

#include "Core/IO/icFile.h"

#include <thread>

/*! c'tor
**/
icFile::icFile(void)
{
    m_ptrUser = NULL;
    m_pVoidCallback = NULL;

    m_u64FilePos = 0;

    m_bStreaming = false;
}// END FUNCTION icFile(void)


/*! d'tor
**/
icFile::~icFile(void)
{
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

    //if (m_pFile)
    {
        // WAIT FOR ANY PENDING ASYNCHRONOUS CALLS
        while (m_bStreaming)
            ;

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

    return IC_WARN_GEN;
}// END FUNCTION ReadAsync(void* pDest, size_t size, void (*callback)(void))


/*! Stops all Asynchronous operations
 *
 *  @returns    ICRESULT        Status after stopping asynchronous ops
**/
ICRESULT icFile::StopAsync(void)
{
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

    return IC_FAIL_GEN;
}// END FUNCTION GetPos(size_t* pos)


/*! Sets the file pointer
 *
 *  @param      pos         Desired file position
 *  @returns    ICRESULT    Status after changing file position
**/
ICRESULT icFile::SetPos(const uint64 pos)
{
    return IC_FAIL_GEN;
}// END FUNCTION SetPos(const uint64 pos)

#endif// ifdef __APPLE__

