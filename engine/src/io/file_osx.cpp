namespace ice::io {
#ifndef WIN32
} // namespace ice::io
#include "ice/io/file.h"
#include <limits>
namespace ice::io {
file::file()
    : fp_(nullptr), file_pos_(0), streaming_(false), file_mode_(0), ptr_user_(nullptr),
      void_callback_(nullptr) {}
file::~file() { close(); }
ice::result file::open(const char *file, ice::uchar mode) {
	close();
	if (!file)
		return IC_FAIL_NPE;
	if (mode & fm_async)
		return IC_NOT_IMPL;
	const bool write  = (mode & fm_write) != 0;
	const bool create = (mode & fm_create_always) != 0;
	const bool always = (mode & fm_open_always) != 0;
	fp_               = fopen(file, create ? "w+b" : write ? "r+b" : "rb");
	if (!fp_ && always)
		fp_ = fopen(file, "w+b");
	if (!fp_)
		return IC_FAIL_GEN;
	file_mode_ = mode;
	return IC_OK;
}
ice::result file::close() {
	ice::result r = IC_OK;
	if (fp_) {
		if (fclose(fp_) != 0)
			r = IC_FAIL_GEN;
		fp_ = nullptr;
	}
	file_mode_ = 0;
	return r;
}
ice::result file::read(void *dest, size_t size, size_t *bytes_read) {
	if (bytes_read)
		*bytes_read = 0;
	if (!fp_ || (!dest && size))
		return IC_FAIL_NPE;
	size_t n = fread(dest, 1, size, fp_);
	if (bytes_read)
		*bytes_read = n;
	return n == size ? IC_OK : IC_FAIL_GEN;
}
ice::result file::write(void *source, size_t size, size_t *bytes_written) {
	if (bytes_written)
		*bytes_written = 0;
	if (!fp_ || (!source && size))
		return IC_FAIL_NPE;
	size_t n = fwrite(source, 1, size, fp_);
	if (bytes_written)
		*bytes_written = n;
	return n == size ? IC_OK : IC_FAIL_GEN;
}
ice::result file::get_size(ice::uint64 *size) {
	if (!fp_ || !size)
		return IC_FAIL_NPE;
	off_t pos = ftello(fp_);
	if (pos < 0 || fseeko(fp_, 0, SEEK_END) != 0)
		return IC_FAIL_GEN;
	off_t end    = ftello(fp_);
	int   result = fseeko(fp_, pos, SEEK_SET);
	if (end < 0 || result != 0)
		return IC_FAIL_GEN;
	*size = (ice::uint64)end;
	return IC_OK;
}
ice::result file::get_pos(ice::uint64 *pos) {
	if (!fp_ || !pos)
		return IC_FAIL_NPE;
	off_t n = ftello(fp_);
	if (n < 0)
		return IC_FAIL_GEN;
	*pos = (ice::uint64)n;
	return IC_OK;
}
ice::result file::set_pos(ice::uint64 pos) {
	if (!fp_ || pos > (ice::uint64)std::numeric_limits<off_t>::max())
		return IC_FAIL_GEN;
	return fseeko(fp_, (off_t)pos, SEEK_SET) == 0 ? IC_OK : IC_FAIL_GEN;
}
ice::result file::read_async(void *dest [[maybe_unused]], size_t size [[maybe_unused]],
                             void *user_ptr [[maybe_unused]],
                             void (*callback [[maybe_unused]])(void *, size_t)) {
	return IC_NOT_IMPL;
}
ice::result file::write_async(void *source [[maybe_unused]], size_t size [[maybe_unused]],
                              void *user_ptr [[maybe_unused]],
                              void (*callback [[maybe_unused]])(void *, size_t)) {
	return IC_NOT_IMPL;
}
ice::result file::stop_async() { return IC_OK; }
#endif

} // namespace ice::io
