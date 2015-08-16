
#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>

#include "Core/icDebug.h"
#include "Core/icGlobals.h"
#include "Memory/icString.h"




enum ICDB_STR
{
    ICDB_STR_OK=0,
    ICDB_FAIL_GEN,
    ICDB_FAIL_NPE,
    ICDB_FAIL_CREATE_WIND,
    ICDB_FAIL_CREATE_DEVICE,
    ICDB_FAIL_NO_DEVICE,
    ICDB_STR_CNT
};

static const char* icResStr[ICDB_STR_CNT] =
{
    "IC_OK",
    "IC_GENERIC_FAILURE",
    "IC_NULL_POINTER_EXCEPTION",
    "IC_FAILED_CREATE_WINDOW",
    "IC_FAILED_CREATE_DEVICE",
    "IC_FAILED_NO_DEVICE",
};

static const char unknownVal[32] = "IC_UNKOWN_RESULT_CODE";

/*! Looks up a string from ICRESULT
 *
 *    @param[in]    res                ICRESULT value to look up
 *    @returns    const char*        Pointer to string value
 *
**/
const char* icGetStr( long res )
{
    const char* resStr=NULL;
    switch( res )
    {
    case IC_OK: resStr=icResStr[ICDB_STR_OK]; break;
    //case IC_FAIL_GEN: resStr=icResStr[ICDB_FAIL_GEN]; break;
    default: resStr = unknownVal;
    }
    return resStr;
}// END FUNCTION icGetStr( ICRESULT res )


/*! Used for fatal errors, called from assert
 *
 *    @param[in]    szFormat    The user string format
 *    @param[in]    ...            List of parameters
 *
**/
void icErrorf(const char* szFormat, ... )
{
    icString buffer = "===========<ERROR>============\n";

    char tempBuff[1024];

    va_list argList;
    va_start(argList, szFormat);

#if (defined(WIN32) && defined(_MSC_VER))
    vsprintf_s(tempBuff, 1024, szFormat, argList);
#else
    vsprintf(tempBuff, szFormat, argList);
#endif


    va_end(argList);

    buffer += tempBuff;

    // make sure we have a carriage return at the end
    if (buffer[buffer.Length()] != '\n')
        buffer += "\n";


    buffer += "===========</ERROR>============\n";

    //! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
    OutputDebugStringA(buffer.ToChar());
#else
    printf(buffer.ToChar());
#endif

    *(int*)0=0;
}// END FUNCTION icErrorf( const char* szFormat, ... )


/*! Used for non fatal cases
 *
 *    @param[in]    szFormat    The user string format
 *    @param[in]    ...            List of parameters
 *
**/
void icWarningf(const char* szFormat, ... )
{
    icString buffer = "===========<WARNING>============\n";

    char tempBuff[1024];

    va_list argList;
    va_start(argList, szFormat);

#if (defined(WIN32) && defined(_MSC_VER))
    vsprintf_s(tempBuff, 1024, szFormat, argList);
#else
    vsprintf(tempBuff, szFormat, argList);
#endif

    va_end(argList);

    buffer += tempBuff;

    // make sure we have a carriage return at the end
    if (buffer[buffer.Length()] != '\n')
        buffer += "\n";

    buffer += "===========</WARNING>============\n";

    //! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
    OutputDebugStringA(buffer.ToChar());
    //MessageBox(NULL, buffer.ToChar(),"Warning",0);
#else
    printf(buffer.ToChar());
#endif
}// END FUNCTION icWarningf(const char* szFormat, ... )


/*! Platform, IDE independant printf
 *
 *    @param[in]    szFormat    The user string format
 *    @param[in]    ...            List of parameters
 *
**/
void icPrintf(const char* szFormat, ... )
{
    char tempBuff[1024];

    va_list argList;
    va_start(argList, szFormat);

#if (defined(WIN32) && defined(_MSC_VER))
    vsprintf_s(tempBuff, 1024, szFormat, argList);
#else
    vsprintf(tempBuff, szFormat, argList);
#endif

    va_end(argList);

    // make sure we have a carriage return at the end
    size_t len = strlen(tempBuff);
    if (len < 1022 && tempBuff[len] != '\n')
    {
        tempBuff[len++] = '\n';
        tempBuff[len] = 0;
    }
	else
	{
		int bollocks = 2;
	}

    //! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
    OutputDebugStringA(tempBuff);
#else
    printf(tempBuff);
#endif

}// END FUNCTION icPrintf( const char* szFormat, ... )


/*! Used for fatal errors, called from assert
 *
 *    @param[in]    szFormat    The user string
**/
void icError(const char* szFormat)
{
    //! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
    OutputDebugStringA(szFormat);
#else
    printf(szFormat);
#endif

    *(int*)0=0;
}// END FUNCTION Error( const char* szFormat, ... )


/*! Used for non fatal cases
 *
 *    @param[in]    szFormat    The user string format
**/
void icWarning(const char* szFormat)
{
    icString buffer = "===========<WARNING>============\n";

    char tempBuff[1024];

#if (defined(WIN32) && defined(_MSC_VER))
    strcpy_s(tempBuff, 1024, szFormat);
#else
    int len = strlen(szFormat);
    len = len < 1024 ? len : 1024;
    memcpy(tempBuff, szFormat, sizeof(char)*len);
#endif

    buffer += tempBuff;

    // make sure we have a carriage return at the end
    if (buffer[buffer.Length()] != '\n')
        buffer += "\n";

    buffer += "===========</WARNING>============\n";

    //! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
    OutputDebugStringA(buffer.ToChar());
    //MessageBox(NULL, buffer.ToChar(),"Warning",0);
#else
    printf(buffer.ToChar());
#endif
}// END FUNCTION icWarning(const char* szFormat)


/*! Platform, IDE independant printf
 *
 *    @param[in]    szFormat    The user string format
**/
void icPrint(const char* szFormat)
{
    //! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
    OutputDebugStringA(szFormat);
#else
    printf(szFormat);
#endif
}// END FUNCTION icPrintf(const char* szFormat)