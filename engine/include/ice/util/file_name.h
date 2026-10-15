#pragma once

#include "ice/globals.h"
#include <string>
namespace ice::util {

/// Owned path string and borrowed component accessors. Mutating the path invalidates previous
/// component pointers.
class file_name {
public:
	file_name(void);
	~file_name(void);

	ice::result parse_file_name(const char *file_name, ice::uint length = 0);
	ice::result parse_file_name(const std::string &s_file_name);

	/// Return the native drive prefix parsed from the filename, or an empty string when absent.
	inline const char *get_drive(void) { return drive_; };
	/// Borrow the directory/path component; changing the owned path invalidates it.
	inline const char *get_path(void) { return path_; };
	/// Borrow the filename component; changing the owned path invalidates it.
	inline const char *get_file_name(void) { return file_name_; };
	/// Borrow the filename extension component; changing the owned path invalidates it.
	inline const char *get_file_extension(void) { return extension_; };

private:
	void split_path(const char *file_name);

	char        mem_[512];  //! Memory pool
	const char *drive_;     //! Pointer to the drive letter
	const char *path_;      //! Pointer to the path
	const char *file_name_; //! Pointer to the filename
	const char *extension_; //! Pointer to the extention
};

} // namespace ice::util
