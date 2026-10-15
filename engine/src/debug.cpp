namespace ice {

#ifdef WIN32
} // namespace ice
#include <windows.h>
namespace ice {
#endif
} // namespace ice

#include <string.h>

#include "ice/debug.h"
#include "ice/globals.h"
#include <cstdlib>
#include <string>
namespace ice {

enum db_str {
	db_str_ok = 0,
	db_fail_gen,
	db_fail_npe,
	db_fail_create_wind,
	db_fail_create_device,
	db_fail_no_device,
	db_str_cnt
};

static const char *result_strings[db_str_cnt] = {
    "IC_OK",
    "IC_GENERIC_FAILURE",
    "IC_NULL_POINTER_EXCEPTION",
    "IC_FAILED_CREATE_WINDOW",
    "IC_FAILED_CREATE_DEVICE",
    "IC_FAILED_NO_DEVICE",
};

static const char unknown_val[32] = "IC_UNKOWN_RESULT_CODE";

/*! Looks up a string from ICRESULT
 *
 *    @param[in]    result_code                ICRESULT value to look up
 *    @returns    const char*        Pointer to string value
 *
 **/
const char *get_str(long result_code) {
	const char *res_str = NULL;
	switch (result_code) {
	case IC_OK:
		res_str = result_strings[db_str_ok];
		break;
	// case IC_FAIL_GEN: resStr=icResStr[ICDB_FAIL_GEN]; break;
	default:
		res_str = unknown_val;
	}
	return res_str;
}

/*! Used for fatal errors, called from assert
 *
 *    @param[in]    format    The user string format
 *    @param[in]    ...            List of parameters
 *
 **/
void errorf(const char *format, ...) {
	std::string buffer = "===========<ERROR>============\n";

	char temp_buff[1024];

	va_list arg_list;
	va_start(arg_list, format);

#if (defined(WIN32) && defined(_MSC_VER))
	vsprintf_s(tempBuff, 1024, szFormat, argList);
#else
	vsnprintf(temp_buff, sizeof(temp_buff), format, arg_list);
#endif

	va_end(arg_list);

	buffer += temp_buff;

	// make sure we have a carriage return at the end
	if ((buffer.empty() || buffer.back() != '\n'))
		buffer += "\n";

	buffer += "===========</ERROR>============\n";

	//! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
	OutputDebugStringA(buffer.c_str());
#else
	::printf("%s", buffer.c_str());
#endif

	std::abort();
}

/*! Used for non fatal cases
 *
 *    @param[in]    format    The user string format
 *    @param[in]    ...            List of parameters
 *
 **/
void warningf(const char *format, ...) {
	std::string buffer = "===========<WARNING>============\n";

	char temp_buff[1024];

	va_list arg_list;
	va_start(arg_list, format);

#if (defined(WIN32) && defined(_MSC_VER))
	vsprintf_s(tempBuff, 1024, szFormat, argList);
#else
	vsnprintf(temp_buff, sizeof(temp_buff), format, arg_list);
#endif

	va_end(arg_list);

	buffer += temp_buff;

	// make sure we have a carriage return at the end
	if ((buffer.empty() || buffer.back() != '\n'))
		buffer += "\n";

	buffer += "===========</WARNING>============\n";

	//! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
	OutputDebugStringA(buffer.c_str());
	// MessageBox(NULL, buffer.c_str(),"Warning",0);
#else
	::printf("%s", buffer.c_str());
#endif
}

/*! Platform, IDE independant printf
 *
 *    @param[in]    format    The user string format
 *    @param[in]    ...            List of parameters
 *
 **/
void printf(const char *format, ...) {
	char temp_buff[1024];

	va_list arg_list;
	va_start(arg_list, format);

#if (defined(WIN32) && defined(_MSC_VER))
	vsprintf_s(tempBuff, 1024, szFormat, argList);
#else
	vsnprintf(temp_buff, sizeof(temp_buff), format, arg_list);
#endif

	va_end(arg_list);

	// make sure we have a carriage return at the end
	size_t len = strlen(temp_buff);
	if (len < 1022 && temp_buff[len] != '\n') {
		temp_buff[len++] = '\n';
		temp_buff[len]   = 0;
	}

	//! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
	OutputDebugStringA(temp_buff);
#else
	::printf("%s", temp_buff);
#endif
}

/*! Used for fatal errors, called from assert
 *
 *    @param[in]    format    The user string
 **/
void error(const char *format) {
	//! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
	OutputDebugStringA(format);
#else
	::printf("%s", format);
#endif

	std::abort();
}

/*! Used for non fatal cases
 *
 *    @param[in]    format    The user string format
 **/
void warning(const char *format) {
	std::string buffer = "===========<WARNING>============\n";

	char temp_buff[1024];

#if (defined(WIN32) && defined(_MSC_VER))
	strcpy_s(tempBuff, 1024, szFormat);
#else
	int len = strlen(format);
	len     = len < 1024 ? len : 1024;
	memcpy(temp_buff, format, sizeof(char) * len);
#endif

	buffer += temp_buff;

	// make sure we have a carriage return at the end
	if ((buffer.empty() || buffer.back() != '\n'))
		buffer += "\n";

	buffer += "===========</WARNING>============\n";

	//! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
	OutputDebugStringA(buffer.c_str());
	// MessageBox(NULL, buffer.c_str(),"Warning",0);
#else
	::printf("%s", buffer.c_str());
#endif
}

/*! Platform, IDE independant printf
 *
 *    @param[in]    format    The user string format
 **/
void print(const char *format) {
	//! TODO: this needs to be platform/IDE independant
#if (defined(WIN32) && defined(_MSC_VER))
	OutputDebugStringA(format);
#else
	::printf("%s", format);
#endif
}
} // namespace ice
