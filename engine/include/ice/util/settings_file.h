#pragma once

#include "ice/globals.h"
#include "ice/io/file.h"
#include <string>
namespace ice::util {

#define ICKEYLENGTH 128
#define ICVALUELENGTH 128
#define IC_SETTINGS_MAX (64)

/// Owned settings key/value entry using textual serialization.
struct key_value {
	/// Owned settings key.
	char key[ICKEYLENGTH];
	/// Owned settings value text.
	char value[ICVALUELENGTH];

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator<(const key_value &rhs) { return strcmp(key, rhs.key) < 0; };

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator>(const key_value &rhs) { return strcmp(key, rhs.key) > 0; };

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator>=(const key_value &rhs) { return (strcmp(key, rhs.key) >= 0); };

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator<=(const key_value &rhs) { return (strcmp(key, rhs.key) <= 0); }

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator==(const key_value &rhs) { return (strcmp(key, rhs.key) == 0); };

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator!=(const key_value &rhs) { return (strcmp(key, rhs.key) != 0); };
};

/*! Provides simple settings file parsing
 **/
class settings_file {
public:
	/// Construct inactive state; initialize explicitly before active use.
	settings_file(void);

	/// Discard owned settings entries without changing external resource ownership.
	void clear_values(void);

	/// Store the native settings path for subsequent read/write operations; a null path fails.
	ice::result set_file(const char *file);
	/// Read into caller-owned output storage; inspect the returned count/status and diagnostics
	/// before consuming data.
	ice::result read(void);
	/// Write caller-provided bytes; inspect the returned count/status for partial or failed writes.
	ice::result write(void);

	/// Decode/read a native file; inspect the result before accessing the resulting data.
	ice::result read_file(const char *file);
	/// Serialize owned data to a native file; inspect the result for write failure.
	ice::result write_file(const char *file);

	/// Copy a key/value into the named settings section; replacing a key updates its value.
	ice::result add_pair(const char *key, const char *value);
	/// Copy/convert the selected settings entry into caller storage.
	std::string get_value(const char *key, const char *def_val = NULL, bool auto_add = true);

private:
	void parse_data(char *mem, int size);

	std::string file_;
	key_value   heap_[IC_SETTINGS_MAX];
	int         num_;
};

} // namespace ice::util
