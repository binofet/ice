namespace ice::assets {
#define CGLTF_IMPLEMENTATION
} // namespace ice::assets
#include "cgltf/cgltf.h"
namespace ice::assets {
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
} // namespace ice::assets
#include "stb/stb_image.h"
