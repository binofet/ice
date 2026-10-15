#pragma once

#include "ice/globals.h"
namespace ice::assets {

#ifdef _MSC_VER
#pragma pack(push) /* push current alignment to stack */
#pragma pack(1)    /* set alignment to 1 byte boundary */
#endif

/*! Targa file header
 */
struct tga_header {
	/// TGA identification-field length in bytes.
	ice::uchar identsize; // size of ID field that follows 18 byte header
	/// TGA palette-present encoding.
	ice::uchar colourmaptype; // type of colour map 0=none, 1=has palette
	/// TGA image/compression encoding.
	ice::uchar imagetype; // type of image

	/// First palette-entry index.
	short colourmapstart; // first colour map entry in palette
	/// Number of palette entries.
	short colourmaplength; // number of colours in palette
	/// Bits per palette entry.
	ice::uchar colourmapbits; // number of bits per palette entry 15,16,24,32

	/// TGA image-origin X coordinate.
	short xstart; // image x origin
	/// TGA image-origin Y coordinate.
	short ystart; // image y origin
	/// Horizontal extent in pixels unless the owning API specifies logical UI units.
	short width; // image width in pixels
	/// Vertical extent in pixels unless the owning API specifies logical UI units.
	short height; // image height in pixels
	/// Bits per stored pixel.
	ice::uchar bits; // image bits per pixel 8,16,24,32
	/// TGA image-origin/alpha descriptor bits.
	ice::uchar descriptor; // image descriptor bits (vh flip bits)
	                       /// pixel data follows header
#ifdef _MSC_VER
};
#else
} __attribute__((packed));
#endif

#ifdef ICGL
} // namespace ice::assets

#include "ice/render/gl/api.h"
namespace ice::assets {

/*! struct to get opengl texture info
 **/
struct gl_tex_info {
	/// Horizontal extent in pixels unless the owning API specifies logical UI units.
	int width;
	/// Vertical extent in pixels unless the owning API specifies logical UI units.
	int height;
	/// Pixel depth or collision penetration, in units defined by the owning record.
	int depth;
	/// Format/token interpreted by the enclosing API.
	GLint format;
	/// Number of decoded/upload color components.
	GLint comp;
};
#endif

#ifdef _MSC_VER
#pragma pack(pop) /* restore original alignment from stack */
#endif
} // namespace ice::assets
namespace ice::io {
class file;
}
namespace ice::assets {
/*! Provides an interface to get device ready bits from textures
 **/
class image {
public:
	/// Create an empty image with no decoded pixel allocation.
	image(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	~image(void);

	/// Read image metadata without decoding pixel storage; inspect the returned status before using
	/// the header.
	ice::result read_header(const char *file);

	/// Return the decoded pixel storage size in bytes.
	ice::result get_image_data_size(ice::uint *size);

	/// Copy decoded pixels into caller-provided storage of at least get_image_data_size bytes.
	ice::result copy_image_bits(void *destination);

#ifdef ICGL
	/// Write legacy GL upload metadata for the currently decoded image.
	ice::result get_gl_tex_info(gl_tex_info *info);
#endif

private:
	ice::io::file *file_;

	bool       header_current_;
	tga_header header_; // temporary, this should be a universal object
};

} // namespace ice::assets
