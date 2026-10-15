#pragma once

#include "ice/io/resource_uri.h"
#include "ice/math/types.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace ice::assets {

/// CPU assets contain no parser objects, graphics handles or backend-specific types.
struct scene_vertex {
	/// Mesh-local position in asset units.
	float position[3] = {};
	/// Mesh-local surface normal.
	float normal[3] = {0, 1, 0};
	/// Mesh-local tangent XYZ; W is the bitangent handedness sign.
	float tangent[4] = {1, 0, 0, 1};
	/// Skin-local joint indices, paired with weights.
	float joints[4] = {};
	/// Four normalized skinning weights.
	float weights[4] = {1, 0, 0, 0};
	/// First and second texture-coordinate sets.
	float uv[2][2] = {{0, 0}, {0, 0}};
	/// Linear RGBA vertex multiplier.
	float color[4] = {1, 1, 1, 1};
};
/// Decoded, tightly packed RGBA8 image. The asset owns its bytes; texture consumers choose color-
/// space interpretation.
struct scene_image {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Decoded image width and height in pixels.
	int width = 0, height = 0;
	/// Owned tightly packed RGBA8 image bytes, with four bytes per pixel.
	std::vector<unsigned char> rgba;
};
/// Asset image index and glTF sampler tokens. A negative image index denotes an absent image.
struct scene_texture {
	/// Index into scene_asset::images; negative means no image.
	int image = -1;
	/// glTF magnification and minification/mipmap sampler tokens; translate in the GPU backend.
	int mag_filter = 9729, min_filter = 9987;
	/// glTF sampler address modes for the first and second texture coordinates.
	int wrap_s = 10497, wrap_t = 10497;
};
/// Material texture reference, UV set, and slot-specific strength/scale.
struct scene_texture_slot {
	/// Index into scene_asset::textures; negative means the slot has no texture.
	int texture = -1;
	/// Vertex texture coordinate set selected by this material slot.
	int uv_set = 0;
	/// Scale factor; for normal/occlusion slots this is the glTF slot strength.
	float scale = 1;
};
/// Material alpha treatment: opaque, alpha-cutoff mask, or blended transparency.
enum class alpha_mode { opaque, mask, blend };
/// glTF metallic/roughness material. Color factors are linear; texture color-space interpretation
/// depends on the slot.
struct scene_material {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Linear RGBA base-color factor.
	float base_color[4] = {1, 1, 1, 1};
	/// Linear emissive RGB factor.
	float emissive[3] = {0, 0, 0};
	/// Metallic factor in [0,1].
	float metallic = 1;
	/// Perceptual roughness in [0,1].
	float roughness = 1;
	/// Alpha threshold when alpha_mode is mask.
	float alpha_cutoff = .5f;
	/// Emissive texture slot; RGB samples use sRGB decoding. Ambient-occlusion slot; red contains
	/// linear occlusion data. Tangent-space normal texture slot; sampled as linear data.
	/// Metallic/roughness texture slot; B/G contain metallic/roughness scalar data. Base-color
	/// texture slot; RGB samples use sRGB decoding.
	scene_texture_slot base_color_map, metal_rough_map, normal_map, occlusion_map, emissive_map;
	/// Material transparency treatment.
	ice::assets::alpha_mode alpha_mode = alpha_mode::opaque;
	/// Use the material unlit path instead of lighting evaluation. Disable material back-face
	/// culling and orient normals for the visible side.
	bool double_sided = false, unlit = false;
};
/// Owned indexed triangle geometry and material reference; joint attributes are local to the bound
/// skin.
struct scene_primitive {
	/// Whether vertices contain skin influences requiring a node skin binding.
	bool skinned = false;
	/// Owned interleaved vertex data for this primitive.
	std::vector<scene_vertex> vertices;
	/// Owned triangle/index data selecting vertex elements.
	std::vector<std::uint32_t> indices;
	/// Asset material index, or a view isolation selector where the type documents -1 as all.
	unsigned material = 0;
};
/// Named collection of primitive indices; scene nodes supply transforms and optional skins.
struct scene_mesh {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Owned primitive records or their indices, as specified by the container type.
	std::vector<unsigned> primitives;
};
/// Imported hierarchy node. TRS rotation is XYZW; matrix nodes retain their authored column-major
/// transform.
struct scene_node {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Skin index bound to this node; negative means no skin. Mesh index bound to this node;
	/// negative means no mesh. Parent node index; a negative value denotes a hierarchy root.
	int parent = -1, mesh = -1, skin = -1;
	/// Use the authored matrix rather than reconstructing the local transform from TRS.
	bool matrix_transform = false;
	/// Local translation in asset units.
	std::array<float, 3> translation{{0, 0, 0}};
	/// Local per-axis scale.
	std::array<float, 3> scale{{1, 1, 1}};
	/// Local unit quaternion in XYZW storage order.
	std::array<float, 4> rotation{{0, 0, 0, 1}};
	/// Owned child-node index list in the asset node array.
	std::vector<unsigned> children;
	/// Column-major local-to-world transform(s). Column-major local-to-parent transform(s).
	ice::math::mat4 local, world;
};
/// Drawable pairing of a scene node and primitive; resources remain owned by the asset.
struct scene_instance {
	/// Index into the owning asset primitive array. Index into the owning asset node array.
	unsigned node = 0, primitive = 0;
};

/// Ordered joint indices and inverse bind transforms. Palette order matches vertex joint indices.
struct scene_skin {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Optional skeleton-root node index; negative means unspecified.
	int skeleton = -1;
	/// Skin-local joint indices; palette ordering follows the bound skin.
	std::vector<unsigned> joints;
	/// Inverse bind matrices ordered to match the skin joint list.
	std::vector<ice::math::mat4> inverse_bind;
};
/// Animated local transform component; rotations use XYZW quaternions.
enum class animation_path { translation, rotation, scale };
/// glTF step, linear, or cubic-spline key interpolation.
enum class animation_interpolation { step, linear, cubic_spline };
/// Animation channel targeting one node/property; times are seconds and cubic data stores
/// in/value/out triples.
struct scene_animation_track {
	/// Index into the owning asset node array.
	unsigned node = 0;
	/// URI path or animated property, as determined by the enclosing record type.
	animation_path path = animation_path::translation;
	/// Interpolation rule used between adjacent animation keys.
	animation_interpolation interpolation = animation_interpolation::linear;
	/// Animation key times in seconds, ordered monotonically.
	std::vector<float> times;
	/// Cubic keys store in-tangent, value, out-tangent; rotations use XYZW.
	std::vector<float> values;
};
/// Named owned animation channels and their start/end times in seconds.
struct scene_animation_clip {
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// End time/distance in the units defined by the enclosing record. Start time/distance in the
	/// units defined by the enclosing record.
	float start = 0, end = 0;
	/// Owned animation channels targeting nodes and local transform properties.
	std::vector<scene_animation_track> tracks;
};
/// Owned backend-independent scene data. Parser storage and native graphics handles never escape
/// into this object.
struct scene_asset {
	/// Origin URI/source payload or input mode, as specified by the enclosing record.
	std::string source;
	/// Owned nonfatal import diagnostics; successful load may still produce warnings. Owned
	/// canonical external resource URIs encountered during loading.
	std::vector<std::string> dependencies, warnings;
	/// Owned decoded image records.
	std::vector<scene_image> images;
	/// Owned texture/sampler records.
	std::vector<scene_texture> textures;
	/// Owned material records.
	std::vector<scene_material> materials;
	/// Owned primitive records or their indices, as specified by the container type.
	std::vector<scene_primitive> primitives;
	/// Owned mesh records.
	std::vector<scene_mesh> meshes;
	/// Owned hierarchy nodes.
	std::vector<scene_node> nodes;
	/// Owned skin bindings.
	std::vector<scene_skin> skins;
	/// Owned animation clips imported with the asset.
	std::vector<scene_animation_clip> animations;
	/// Root node indices for the selected scene.
	std::vector<unsigned> roots;
	/// Drawable node/primitive pairings for the selected scene.
	std::vector<scene_instance> instances;
	/// Minimum corner of the imported world-space bounds.
	float bounds_min[3] = {};
	/// Maximum corner of the imported world-space bounds.
	float bounds_max[3] = {};
};
/// Transactional: failure leaves output intact. Core glTF 2.0 / GLB, TRS animation and four-weight
/// skins. PNG/JPEG, embedded/data/external resources, UV0/1, triangle lists/strips/fans.
bool load_gltf(const std::string &path, scene_asset &output, std::string &diagnostics);

/// Load a glTF/GLB scene transactionally through the URI resolver. False records diagnostics and
/// preserves the previous output asset.
bool load_gltf(const std::string &uri, ice::io::resource_resolver &resolver, scene_asset &output,
               std::string &diagnostics);

} // namespace ice::assets
