#include <cctype>

#include "ice/assets/image.h"

#include "ice/globals.h"
#include "ice/io/file.h"
#include "ice/util/file_name.h"
#include <string>
namespace ice::assets {

image::image(void) {
	file_           = NULL;
	header_current_ = false;
}

image::~image(void) {
	if (file_)
		SAFEDEL(file_);
}

ice::result image::read_header(const char *file) {
	if (file_)
		SAFEDEL(file_);

	file_ = new ice::io::file();

	if (ICEFAIL(file_->open(file, ICFMREAD_EXISTING))) {
		SAFEDEL(file_);
		ice::warningf("icImage could not open file (%s)", file);
		return IC_FAIL_GEN;
	}

	ice::util::file_name file_name;
	file_name.parse_file_name(file);

	std::string ext = file_name.get_file_extension();
	for (char &c : ext)
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

	if (ext == ".TGA") {
		size_t bytes_read = 0;
		if (ICEFAIL(file_->read(&header_, sizeof(header_), &bytes_read))) {
			ice::warningf("Failure loading image header: %s", file);
			return IC_FAIL_GEN;
		}
		header_current_ = true;
		return IC_OK;
	}
	// else if (ext == "BMP")
	//{
	// }
	// else if (ext == "JPG" || ext == "JPEG")
	//{
	// }

	ice::warningf("Attempt to load invalid image type: %s", file);
	return IC_FAIL_GEN;
}

ice::result image::get_image_data_size(ice::uint *size) {
	if (!header_current_) {
		*size = 0;
		return IC_FAIL_GEN;
	}

	short width  = header_.width;
	short height = header_.height;
	char  depth  = header_.bits / 8;

	*size = (ice::uint)width * height * depth;

	return IC_OK;
}

ice::result image::copy_image_bits(void *destination) {
	if (!header_current_)
		return IC_FAIL_GEN;

	// save off important stuff to callers pTex
	short width  = header_.width;
	short height = header_.height;
	char  depth  = header_.bits / 8;

	// get some memory and fill it up
	size_t size = (size_t)width * height * depth;

	size_t bytes_read = 0;

	ice::result result_code = file_->read(destination, size, &bytes_read);

	return result_code;
}

#ifdef ICGL
ice::result image::get_gl_tex_info(gl_tex_info *info) {
	if (!header_current_)
		return IC_FAIL_GEN;

	// save off important stuff to callers pTex
	info->width  = header_.width;
	info->height = header_.height;
	info->depth  = header_.bits / 8;

	switch (info->depth) {
	case 1: {
		info->format = GL_LUMINANCE;
		info->comp   = GL_LUMINANCE8;
	} break;
	case 3: {
		info->format = GL_BGR_EXT;
		info->comp   = GL_RGB8;
	} break;
	case 4: {
		info->format = GL_BGRA_EXT;
		info->comp   = GL_RGBA8;
	} break;
	default:
		ice::warning("trying to load unsupported texture!");
		return IC_FAIL_GEN;
	};

	return IC_OK;
}
#endif
} // namespace ice::assets
