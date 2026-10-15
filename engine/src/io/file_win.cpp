namespace ice::io {

#ifdef WIN32
} // namespace ice::io

#include "ice/io/file.h"
namespace ice::io {

/*! Function converts ICE file modes to Microsoft flags
 *
 *  @param      mode      8-bit mask of ICFILEMODE flags
 *  @param[out] access     Pointer for access rights
 *  @param[out] creation    Pointer for creation disposition
 *  @param[out] flags      Pointer for flags and attributes
 **/
void get_file_params(ice::uchar mode, DWORD *access, DWORD *creation, DWORD *flags) {
	*access = 0;
	*flags  = 0;

	if (mode & icfm_read)
		*access |= GENERIC_READ;
	if (mode & fm_write)
		*access |= GENERIC_WRITE;

	if (mode & fm_create_always)
		*creation = CREATE_ALWAYS;
	if (mode & fm_create_new)
		*creation = CREATE_NEW;
	if (mode & fm_open_always)
		*creation = OPEN_ALWAYS;
	if (mode & fm_open_existing)
		*creation = OPEN_EXISTING;

	if (mode & fm_async)
		*flags |= FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING;
}

/*! Asynchronous operation callback
 *
 *
 *  @param      error_code     Error flags
 *  @param      transferred_bytes    The number of bytes transferred
 *  @param      overlapped    The overlapped structure for async op
 **/
VOID CALLBACK file::async_callback(DWORD error_code, DWORD transferred_bytes,
                                   LPOVERLAPPED overlapped) {
	if (overlapped) {
		file *file_handle_object = static_cast<file *>(overlapped);

		// check for errors
		if (error_code) {
			// should probably take a look at better way to handle this
			ice::warning("There was an error returned from Async Operation");
		}

		file_handle_object->file_pos_ += transferred_bytes;
		file_handle_object->streaming_ = false;

		// call user callback function with their pointer
		if (file_handle_object->void_callback_)
			(*file_handle_object->void_callback_)(file_handle_object->ptr_user_,
			                                      (size_t)transferred_bytes);
	} else {
		// this should likely never happen
		ice::error("Undefined behavior in Asynchronous callback");
	}
}

/*! c'tor
 **/
file::file(void) {
	ZeroMemory(this, sizeof(file));

	file_handle_   = NULL;
	ptr_user_      = NULL;
	void_callback_ = NULL;

	file_pos_ = 0;

	streaming_ = false;
}

/*! d'tor
 **/
file::~file(void) {
	if (file_handle_) {
		stop_async();

		if (hEvent)
			CloseHandle(hEvent);

		CloseHandle(file_handle_);
	}
}

/*! Opens a file
 *
 *  @param      file      Name of file to open
 *  @param      mode      File mode (read/write/etc)
 *  @returns    ICRESULT    Status after open
 **/
ice::result file::open(const char *file, ice::uchar mode) {
	close();

	DWORD dwAccess     = 0;
	DWORD dwShareMode  = FILE_SHARE_READ; //! should this be exposed?
	DWORD dwCreateDisp = 0;
	DWORD dwFandA      = 0;

	file_mode_ = mode;
	get_file_params(mode, &dwAccess, &dwCreateDisp, &dwFandA);

	file_handle_ = CreateFileA(file,         // LPCSTR
	                           dwAccess,     // Desired Access
	                           dwShareMode,  // Share Mode
	                           NULL,         // lpSecurityAttributes
	                           dwCreateDisp, // Creation Disposition
	                           dwFandA,      // Flags and attributes
	                           NULL);        // HANDLE to template file

	if (file_handle_ && file_handle_ != INVALID_HANDLE_VALUE)
		return IC_OK;
	return IC_FAIL_GEN;
}

/*! Closes the file
 *
 *      This will stall in the event there is an asynchronous operation
 *      still in progress.
 *
 *  @returns    ICRESULT        Status after closing the file
 **/
ice::result file::close(void) {
	file_mode_     = 0;
	ptr_user_      = NULL;
	void_callback_ = NULL;
	file_pos_      = 0;

	if (file_handle_) {
		// WAIT FOR ANY PENDING ASYNCHRONOUS CALLS
		while (streaming_)
			SleepEx(50, TRUE);

		if (hEvent) {
			CloseHandle(hEvent);
			hEvent = NULL;
		}

		if (CloseHandle(file_handle_)) {
			file_handle_ = NULL;
			return IC_OK;
		}
		file_handle_ = NULL;
		return IC_FAIL_GEN;
	}

	streaming_ = false;

	return IC_OK;
}

/*! Reads data from file
 *
 *      Note: This should not be called by ASYNC File objects
 *
 *  @param      dest       Destination buffer
 *  @param      size        Size in bytes to read
 *  @param      bytes_read    Pointer to store size actually read
 *  @returns    ICRESULT    Status after file read
 **/
ice::result file::read(void *dest, size_t size, size_t *bytes_read) {
	if (!bytes_read || (!dest && size))
		return IC_FAIL_NPE;
	*bytes_read = 0;
	if (size > MAXDWORD)
		return IC_FAIL_GEN;
	DWORD transferred = 0;
	if (file_handle_ && !(file_mode_ & fm_async)) {
		if (ReadFile(file_handle_, dest, (DWORD)size, &transferred, NULL)) {
			// Win32 writes a DWORD count, while the public API returns size_t.
			*bytes_read = transferred;
			file_pos_ += transferred;
			return IC_OK;
		}
	}
	return IC_WARN_GEN;
}

/*! Asynchronous Read
 *
 *
 *
 *  @param      dest       Destination buffer
 *  @param      size        Size in bytes to read
 *  @param      user_ptr     Pointer user can use as needed
 *  @param      callback    Function to call when read is finished
 *  @returns    ICRESULT    Status after starting the async-read
 **/
ice::result file::read_async(void *dest, size_t size, void *user_ptr,
                             void (*callback)(void *, size_t)) {
	if (file_handle_ && !streaming_) {
		void_callback_ = callback;
		ptr_user_      = user_ptr;
		streaming_     = true;
		hEvent         = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (ReadFileEx(file_handle_, dest, size, (LPOVERLAPPED)this, async_callback))
			return IC_OK;
		streaming_ = false;
	}
	return IC_WARN_GEN;
}

/*! Stops all Asynchronous operations
 *
 *  @returns    ICRESULT        Status after stopping asynchronous ops
 **/
ice::result file::stop_async(void) {
	if (file_handle_ && streaming_) {
		if (CancelIo(file_handle_))
		// if (CancelIoEx(file_handle_, this))
		{
			streaming_ = false;
			return IC_OK;
		}
	}
	return IC_WARN_GEN;
}

/*! Writes data to a file
 *
 *  @param      source     Pointer to data to be written to file
 *  @param      size        Size of data (in bytes) to write
 *  @param[out] bytes_written Size of data actually written to the file
 *  @returns    ICRESULT    Status after the file write
 **/
ice::result file::write(void *source, size_t size, size_t *bytes_written) {
	if (!bytes_written || (!source && size))
		return IC_FAIL_NPE;
	*bytes_written = 0;
	if (size > MAXDWORD)
		return IC_FAIL_GEN;
	DWORD transferred = 0;
	if (file_handle_ && !(file_mode_ & fm_async)) {
		if (WriteFile(file_handle_, source, (DWORD)size, &transferred, NULL)) {
			// Win32 writes a DWORD count, while the public API returns size_t.
			*bytes_written = transferred;
			file_pos_ += transferred;
			return IC_OK;
		}
	}
	return IC_FAIL_GEN;
}

/*! Writes to a file asynchronously
 *
 *  @param      source     Pointer to data to be written to file
 *  @param      size        Size of data (in bytes) to write
 *  @param      user_ptr     Pointer for user to use in callback
 *  @param      callback    Function pointer for user callback
 *  @returns    ICRESULT    Status after the file write
 **/
ice::result file::write_async(void *source, size_t size, void *user_ptr,
                              void (*callback)(void *, size_t)) {
	if (file_handle_ && !streaming_) {
		void_callback_ = callback;
		ptr_user_      = user_ptr;
		streaming_     = true;
		hEvent         = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (WriteFileEx(file_handle_, source, (DWORD)size, (LPOVERLAPPED)this, async_callback)) {
			FlushFileBuffers(file_handle_);
			return IC_OK;
		} else {
			streaming_ = false;
			DWORD err  = GetLastError();
			ice::warningf("icFile::WriteAsync failed with error: %i", err);
		}
	}
	return IC_FAIL_GEN;
}
//                         void* userPtr, void (*callback)(void*,size_t))

/*! Get File Size in bytes
 *
 *  @param      size        Pointer to store file size
 *  @returns    ICRESULT    Status after getting size
 **/
ice::result file::get_size(ice::uint64 *size) {
	if (file_handle_) {
		DWORD high = 0;
		DWORD low  = GetFileSize(file_handle_, &high);

		// check fail condition
		if (low != INVALID_FILE_SIZE) {
			*size = (ice::uint64)low | (ice::uint64)high << 32;
			return IC_OK;
		}
	}
	return IC_FAIL_GEN;
}

/*! Get the current file position
 *
 *
 *  @param      pos         Pointer to store file position
 *  @returns    ICRESULT    Status after getting file position
 **/
ice::result file::get_pos(ice::uint64 *pos) {
	if (file_handle_) {
		*pos = file_pos_;
		return IC_OK;
	}
	return IC_FAIL_GEN;
}

/*! Sets the file pointer
 *
 *  @param      pos         Desired file position
 *  @returns    ICRESULT    Status after changing file position
 **/
ice::result file::set_pos(const ice::uint64 pos) {
	if (file_handle_) {
#if 1
		LARGE_INTEGER liPos;
		liPos.QuadPart = pos;
		if (SetFilePointerEx(file_handle_, liPos, (PLARGE_INTEGER)&file_pos_, FILE_BEGIN))
#else
		long liPos = (long)pos;
		if (SetFilePointer(file_handle_, liPos, 0, FILE_BEGIN))
#endif
			return IC_OK;
	}
	::MessageBoxA(NULL, "Failed to set file position", "Shit Ballz", 0);
	return IC_FAIL_GEN;
}

#endif // ifdef WIN32

} // namespace ice::io
