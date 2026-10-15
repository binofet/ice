
#include "ice/util/file_name.h"

#include <string.h>
namespace ice::util {

/*! Default c'tor
 **/
file_name::file_name(void) {
	mem_[0]    = '\0';
	drive_     = NULL;
	path_      = NULL;
	file_name_ = NULL;
	extension_ = NULL;
} // END FUCTION icFileName( void )

/*! Default d'tor
 **/
file_name::~file_name(void) {}

/*! Parses the given filename
 *
 *	Finds the drive name, path, filename, and file extension
 *
 *	@param[in]	file_name	Fullpath filename
 *	@param		length		Length of the string, if 0 we find it
 *	@returns	ICRESULT	Result of parse
 *
 **/
ice::result file_name::parse_file_name(const char *file_name, ice::uint length) {
	if (!file_name)
		return IC_FAIL_NPE;

	mem_[0]    = '\0';
	drive_     = NULL;
	path_      = NULL;
	file_name_ = NULL;
	extension_ = NULL;

	if (!length) {
		while (file_name[length] != '\0')
			length++;
	}

	if (length >= sizeof(mem_) - 8)
		return IC_FAIL_GEN;

	// shortest possible filename is x.x
	if (length >= 3) {
		char     *mem = mem_;
		ice::uint np  = 0;
#ifdef WIN32
		// Get the Drive
		if (file_name[1] == ':' && (file_name[2] == '/' || file_name[2] == '\\')) {
			memcpy(mem_, file_name, 3);
			drive_ = mem;
			mem[3] = '\0';
			np++;
			mem += 4;
		}
#endif
		ice::uint l_slash = length - 1;
		// Get the Path and filename
		while (!path_ && l_slash > 1) {
			if (file_name[l_slash] == '/' || file_name[l_slash] == '\\') {
				memcpy(mem, &file_name[(mem - mem_) - np], length - (mem - mem_));
				path_ = mem;
				mem += l_slash - (mem - mem_) + np + 1;
				mem[0] = '\0';
				mem++;
				file_name_ = mem;
				memcpy(mem, &file_name[l_slash + 1], length - l_slash);
			}
			l_slash--;
		}

		if (!file_name_) {
			memcpy(mem, &file_name[(mem - mem_) - np], length - (mem - mem_));
			file_name_ = mem;
			l_slash    = 0;
		}

		ice::uint ext_pos = length - 1;
		// Get the extension
		while (!extension_ && ext_pos > 0) {
			if (file_name[ext_pos--] == '.') {
				mem += strlen(file_name_);
				mem -= length - ext_pos - 1;
				mem[0] = '\0';
				mem++;
				memcpy(mem, &file_name[ext_pos + 1], length - ext_pos + 1);
				extension_                = mem;
				mem[length - ext_pos + 1] = '\0';
			}
		}
	}

	return IC_OK;
}

/*! Parses the given filename
 *
 *	Finds the drive name, path, filename, and file extension
 *
 *	@param[in]	s_file_name	Fullpath filename
 *	@returns	ICRESULT	Result of parse
 *
 **/
ice::result file_name::parse_file_name(const std::string &s_file_name) {
	return parse_file_name(s_file_name.c_str(), s_file_name.size());
}

} // namespace ice::util
