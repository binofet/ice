#pragma once

#include "ice/globals.h"
namespace ice::io {

/// Bit mask combining native file access and creation modes; asynchronous access is platform
/// dependent.
enum filemode {
	icfm_invalid = 0,

	icfm_read = (1 << 0), //! Read file rights
	fm_write  = (1 << 1), //! Write file rights

	fm_create_always = (1 << 2), //! Creates file, always.
	fm_create_new    = (1 << 3), //! Creates file, if not already exist.

	fm_open_always   = (1 << 4), //! Opens a file, always.
	fm_open_existing = (1 << 5), //! Opens a file, only if it exists.

	fm_async = (1 << 6) //! can ONLY use Asynchronous calls
};

#define ICFMREAD_CREATE (ice::io::icfm_read | ice::io::fm_open_always)
#define ICFMREAD_EXISTING (ice::io::icfm_read | ice::io::fm_open_existing)
#define ICFMREAD_NEW (ice::io::icfm_read | ice::io::fm_open_always)

#define ICFMWRITE_CREATE (ice::io::fm_write | ice::io::fm_open_always)
#define ICFMWRITE_EXISTING (ice::io::fm_write | ice::io::fm_open_existing)
#define ICFMWRITE_NEW (ice::io::fm_write | ice::io::fm_create_always)

#define ICFMREAD_WRITE_NEW (ice::io::icfm_read | ice::io::fm_write | ice::io::fm_create_always)
#define ICFMREAD_WRITE_CREATE (ice::io::icfm_read | ice::io::fm_write | ice::io::fm_open_always)
#define ICFMREAD_WRITE_EXIST (ice::io::icfm_read | ice::io::fm_write | ice::io::fm_open_existing)

/*! Class provides file i/o
 **/
#if defined(WIN32)
class file : private OVERLAPPED
#else
/// Owned native file handle. Close or destroy after use; asynchronous operations are currently
/// supported only on Windows.
class file
#endif
{
public:
	/// Create a closed native file object; open before reading or writing.
	file(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~file(void);

	/// Open a native path using the requested access/creation mask; failure leaves the object
	/// closed.
	ice::result open(const char *file, ice::uchar mode);
	/// Close the native file handle after pending operations have completed.
	ice::result close(void);

	/// Read into caller-owned output storage; inspect the returned count/status and diagnostics
	/// before consuming data.
	ice::result read(void *dest, size_t size, size_t *bytes_read);
	/// Begin an asynchronous read into caller-owned storage; keep storage/context alive until
	/// completion or cancellation.
	ice::result read_async(void *dest, size_t size, void *user_ptr,
	                       void (*callback)(void *, size_t));

	/// Write caller-provided bytes; inspect the returned count/status for partial or failed writes.
	ice::result write(void *source, size_t size, size_t *bytes_written);
	/// Begin an asynchronous write from caller storage; keep storage/context alive until completion
	/// or cancellation.
	ice::result write_async(void *source, size_t size, void *user_ptr,
	                        void (*callback)(void *, size_t));

	/// Cancel/wait for outstanding native asynchronous file operations before releasing their
	/// storage.
	ice::result stop_async(void);

	/// Return/copy the current logical extent or byte size according to the owning interface.
	ice::result get_size(ice::uint64 *size);
	/// Return/copy the current native cursor/file position in the units defined by this interface.
	ice::result get_pos(ice::uint64 *pos);

	/// Set the cursor/file position using logical coordinates or byte offsets, as specified by the
	/// interface.
	ice::result set_pos(const ice::uint64 pos);

private:
#if defined(WIN32)
	static VOID CALLBACK async_callback(DWORD error_code, DWORD transferred_bytes,
	                                    LPOVERLAPPED overlapped);

	HANDLE file_handle_; //! File Handle
#else
	FILE *fp_;
#endif

	[[maybe_unused]] ice::uint64 file_pos_; //! Current File Position

	[[maybe_unused]] volatile bool streaming_; //! Asynchronous operation taking place
	ice::uchar                     file_mode_; //! File mode, attributes, creation

	[[maybe_unused]] void *ptr_user_;
	[[maybe_unused]] void (*void_callback_)(void *, size_t);
};

} // namespace ice::io
