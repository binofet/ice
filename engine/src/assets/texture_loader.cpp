#include "ice/assets/content_loader.h"
#include <algorithm>

#include "ice/render/texture.h"
namespace ice::assets {

/*! Texture loader
 *
 *  @param          file_name      Name of the texture to load
 *  @param[in/out]  out_object           Storage place for loaded texture
 *  @returns        ICRESULT        Success/failure of texture load
 **/
template <>
ice::result content_loader::load<ice::render::texture>(const char            *file_name,
                                                       ice::render::texture **out_object) {
	std::string normalized_path(file_name);
	std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
	// TODO: reference counted objects!
	return device_->create_tex_from_file(out_object, normalized_path.c_str());
	// return IC_OK;
}

/*! Texture unloader
 *
 *  @param[in/out]  out_object           Loaded texture
 *  @returns        ICRESULT        Success/failure of texture unload
 **/
template <>
ice::result content_loader::unload<ice::render::texture>(ice::render::texture **out_object) {
	if (device_) {
		// TODO: reference counting!
		// if (refcount == 0)
		device_->release_tex(*out_object);

		*out_object = NULL;
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

} // namespace ice::assets
