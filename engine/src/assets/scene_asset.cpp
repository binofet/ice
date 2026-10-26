#include "ice/assets/scene_asset.h"
#include "cgltf/cgltf.h"
#include "ice/animation/scene_animation.h"
#include "ice/core/utf8.h"
#include "stb/stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
namespace ice::assets {

namespace {
const std::size_t budget = 256u * 1024u * 1024u;
void              require(bool value, const std::string &why) {
    if (!value)
        throw std::runtime_error(why);
}
struct alloc {
	std::size_t used = 0;
};
struct alignas(std::max_align_t) block {
	std::size_t size;
};
void *allocate(void *u, cgltf_size n) {
	auto &a = *static_cast<alloc *>(u);
	if (n > budget - a.used)
		return nullptr;
	auto b = static_cast<block *>(std::malloc(sizeof(block) + n));
	if (!b)
		return nullptr;
	b->size = n;
	a.used += n;
	return b + 1;
}
void release(void *u, void *p) {
	if (!p)
		return;
	auto b = static_cast<block *>(p) - 1;
	static_cast<alloc *>(u)->used -= b->size;
	std::free(b);
}
struct files {
	std::vector<std::string>    paths;
	std::size_t                 bytes    = 0;
	ice::io::resource_resolver *resolver = nullptr;
	std::string                 error;
};
cgltf_result read_file(const cgltf_memory_options *mem, const cgltf_file_options *opt,
                       const char *path, cgltf_size *size, void **data) {
	auto                      &f = *static_cast<files *>(opt->user_data);
	std::vector<unsigned char> resource;
	if (!f.resolver->read(path, resource, f.error))
		return cgltf_result_io_error;
	if (resource.size() > budget - f.bytes)
		return cgltf_result_out_of_memory;
	cgltf_size actual = resource.size();
	if (*size && *size > actual)
		return cgltf_result_data_too_short;
	cgltf_size count = *size ? *size : actual;
	void      *bytes = mem->alloc_func(mem->user_data, count);
	if (!bytes)
		return cgltf_result_out_of_memory;
	if (count)
		std::memcpy(bytes, resource.data(), count);
	*data = bytes;
	*size = count;
	f.bytes += count;
	f.paths.push_back(path);
	return cgltf_result_success;
}
void release_file(const cgltf_memory_options *mem,
                  const cgltf_file_options *options [[maybe_unused]], void *data,
                  cgltf_size size [[maybe_unused]]) {
	mem->free_func(mem->user_data, data);
}
void normalize(float *n) {
	float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
	if (l > 1e-12f)
		for (int k = 0; k < 3; k++)
			n[k] /= l;
	else {
		n[0] = 0;
		n[1] = 1;
		n[2] = 0;
	}
}
void cross(const float *a, const float *b, float *n) {
	n[0] = a[1] * b[2] - a[2] * b[1];
	n[1] = a[2] * b[0] - a[0] * b[2];
	n[2] = a[0] * b[1] - a[1] * b[0];
}
float dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
std::vector<float> unpack(const cgltf_accessor *a, int components) {
	require(a && a->count && a->count <= 2000000, "Invalid or excessive accessor count");
	require((int)cgltf_num_components(a->type) == components, "Unexpected accessor shape");
	std::vector<float> values(a->count * components);
	require(cgltf_accessor_unpack_floats(a, values.data(), values.size()) == values.size(),
	        "Cannot unpack accessor");
	for (float x : values)
		require(std::isfinite(x), "Non-finite vertex data");
	return values;
}
std::string decoded_uri(const char *source) {
	std::string uri(source);
	// cgltf retains JSON escapes in string storage. Decode exactly once, before URI
	// normalization; a URI containing a literal backslash must not be decoded twice.
	uri.resize(cgltf_decode_string(uri.data()));
	require(ice::core::valid_utf8_text(uri, budget), "Resource URI contains invalid UTF-8 or NUL");
	return uri;
}
std::vector<unsigned char> image_bytes(const cgltf_image &image, const std::string &directory,
                                       const cgltf_options &opt, files &files) {
	if (image.buffer_view) {
		auto view  = image.buffer_view;
		auto bytes = cgltf_buffer_view_data(view);
		require(bytes, "Missing image buffer view");
		return {bytes, bytes + view->size};
	}
	require(image.uri, "Image has no source");
	std::string uri = decoded_uri(image.uri);
	if (uri.compare(0, 5, "data:") == 0) {
		auto comma = uri.find(',');
		require(comma != std::string::npos &&
		            uri.substr(0, comma).find(";base64") != std::string::npos,
		        "Only base64 data images supported");
		std::size_t encoded = uri.size() - comma - 1;
		require(encoded && encoded % 4 == 0, "Invalid base64 image length");
		std::size_t n = encoded / 4 * 3;
		if (uri.back() == '=')
			--n;
		if (uri[uri.size() - 2] == '=')
			--n;
		void *bytes = nullptr;
		require(n < budget && cgltf_load_buffer_base64(&opt, n, uri.c_str() + comma + 1, &bytes) ==
		                          cgltf_result_success,
		        "Invalid base64 image");
		std::vector<unsigned char> out((unsigned char *)bytes, (unsigned char *)bytes + n);
		opt.memory.free_func(opt.memory.user_data, bytes);
		return out;
	}
	ice::io::resource_uri base, resolved;
	std::string           error;
	require(ice::io::resource_uri::parse(directory, base, error) &&
	            ice::io::resource_uri::resolve(base, uri, resolved, error),
	        "Invalid image URI: " + error);
	cgltf_size size  = 0;
	void      *data  = nullptr;
	auto read_result = read_file(&opt.memory, &opt.file, resolved.document().c_str(), &size, &data);
	require(read_result == cgltf_result_success,
	        "Cannot read image: " + resolved.string() + " " + files.error);
	std::vector<unsigned char> out((unsigned char *)data, (unsigned char *)data + size);
	release_file(&opt.memory, &opt.file, data, size);
	return out;
}
scene_texture_slot slot(const cgltf_texture_view &v, const cgltf_data &data) {
	scene_texture_slot s;
	if (!v.texture)
		return s;
	require(!v.has_transform, "KHR_texture_transform is not supported yet");
	require(v.texcoord >= 0 && v.texcoord <= 1, "Only TEXCOORD_0 and TEXCOORD_1 are supported");
	require(std::isfinite(v.scale), "Non-finite texture scale");
	s.texture = (int)(v.texture - data.textures);
	s.uv_set  = v.texcoord;
	s.scale   = v.scale;
	return s;
}
void tangents(scene_primitive &primitive, int uv_set) {
	std::vector<std::array<float, 3>> tan(primitive.vertices.size()),
	    bitan(primitive.vertices.size());
	for (std::size_t i = 0; i < primitive.indices.size(); i += 3) {
		auto  a = primitive.indices[i], b = primitive.indices[i + 1], c = primitive.indices[i + 2];
		auto &va = primitive.vertices[a];
		auto &vb = primitive.vertices[b];
		auto &vc = primitive.vertices[c];
		float e1[3], e2[3];
		for (int k = 0; k < 3; k++) {
			e1[k] = vb.position[k] - va.position[k];
			e2[k] = vc.position[k] - va.position[k];
		}
		float du1 = vb.uv[uv_set][0] - va.uv[uv_set][0], dv1 = vb.uv[uv_set][1] - va.uv[uv_set][1],
		      du2 = vc.uv[uv_set][0] - va.uv[uv_set][0], dv2 = vc.uv[uv_set][1] - va.uv[uv_set][1],
		      det = du1 * dv2 - du2 * dv1;
		if (std::fabs(det) < 1e-10f)
			continue;
		for (auto index : {a, b, c})
			for (int k = 0; k < 3; k++) {
				tan[index][k] += (e1[k] * dv2 - e2[k] * dv1) / det;
				bitan[index][k] += (e2[k] * du1 - e1[k] * du2) / det;
			}
	}
	for (std::size_t i = 0; i < primitive.vertices.size(); i++) {
		auto &v          = primitive.vertices[i];
		float projection = dot(v.normal, tan[i].data());
		for (int k = 0; k < 3; k++)
			v.tangent[k] = tan[i][k] - v.normal[k] * projection;
		if (dot(v.tangent, v.tangent) < 1e-12f) {
			float axis[3] = {0, 0, 1};
			if (std::fabs(v.normal[2]) > .9f) {
				axis[0] = 1;
				axis[2] = 0;
			}
			cross(axis, v.normal, v.tangent);
		}
		normalize(v.tangent);
		float b[3];
		cross(v.normal, v.tangent, b);
		v.tangent[3] = dot(b, bitan[i].data()) < 0 ? -1.f : 1.f;
	}
}
} // namespace

bool load_gltf(const std::string &path, scene_asset &output, std::string &diagnostics) {
	ice::io::resource_resolver resolver;
	return load_gltf(path, resolver, output, diagnostics);
}
bool load_gltf(const std::string &uri, ice::io::resource_resolver &resolver, scene_asset &output,
               std::string &diagnostics) {
	diagnostics.clear();
	ice::io::resource_uri source;
	if (!resolver.canonical(uri, source, diagnostics))
		return false;
	const std::string path = source.document();
	alloc             memory;
	files             files;
	files.resolver = &resolver;
	cgltf_options options{};
	options.memory     = {allocate, release, &memory};
	options.file       = {read_file, release_file, &files};
	cgltf_data *parsed = nullptr;
	try {
		auto parse_result = cgltf_parse_file(&options, path.c_str(), &parsed);
		require(parse_result == cgltf_result_success,
		        "Cannot parse glTF/GLB: " + path + " " + files.error);
		std::unique_ptr<cgltf_data, void (*)(cgltf_data *)> guard(parsed, cgltf_free);
		require(parsed->asset.version && std::string(parsed->asset.version) == "2.0",
		        "Only glTF 2.0 is supported");
		require(parsed->nodes_count < 10000 && parsed->meshes_count < 10000 &&
		            parsed->images_count < 256,
		        "Asset resource count limit exceeded");
		for (cgltf_size i = 0; i < parsed->extensions_required_count; i++)
			require(std::string(parsed->extensions_required[i]) == "KHR_materials_unlit",
			        "Unsupported required extension: " +
			            std::string(parsed->extensions_required[i]));
		for (cgltf_size i = 0; i < parsed->buffers_count; i++) {
			auto &buffer = parsed->buffers[i];
			if (!buffer.uri) {
				require(i == 0 && parsed->bin && parsed->bin_size >= buffer.size,
				        "Missing GLB binary data");
				buffer.data = const_cast<void *>(parsed->bin);
				continue;
			}
			std::string buffer_uri = decoded_uri(buffer.uri);
			if (buffer_uri.compare(0, 5, "data:") == 0) {
				auto comma = buffer_uri.find(',');
				require(comma != std::string::npos &&
				            buffer_uri.substr(0, comma).find(";base64") != std::string::npos &&
				            buffer.size <= (buffer_uri.size() - comma - 1) / 4 * 3,
				        "Invalid buffer data URI");
				require(cgltf_load_buffer_base64(&options, buffer.size,
				                                 buffer_uri.c_str() + comma + 1,
				                                 &buffer.data) == cgltf_result_success,
				        "Cannot decode buffer data URI");
				buffer.data_free_method = cgltf_data_free_method_memory_free;
			} else {
				ice::io::resource_uri resolved;
				std::string           error;
				require(ice::io::resource_uri::resolve(source, buffer_uri, resolved, error),
				        "Invalid buffer URI: " + error);
				cgltf_size size        = buffer.size;
				auto       read_result = read_file(&options.memory, &options.file,
				                                   resolved.document().c_str(), &size, &buffer.data);
				require(read_result == cgltf_result_success,
				        "Cannot read buffer " + resolved.string() + " " + files.error);
				buffer.data_free_method = cgltf_data_free_method_memory_free;
			}
		}
		for (cgltf_size i = 0; i < parsed->accessors_count; i++)
			require(parsed->accessors[i].count > 0 && parsed->accessors[i].count <= 6000000,
			        "Accessor count outside import limits");
		for (cgltf_size i = 0; i < parsed->buffer_views_count; i++) {
			auto &v = parsed->buffer_views[i];
			require(v.buffer && v.offset <= v.buffer->size && v.size <= v.buffer->size - v.offset,
			        "Buffer view exceeds its buffer");
			require(!v.has_meshopt_compression, "Meshopt compression is unsupported");
		}
		for (cgltf_size i = 0; i < parsed->accessors_count; i++) {
			auto &a = parsed->accessors[i];
			require(a.stride <= 4096, "Accessor stride exceeds import limit");
			if (a.buffer_view)
				require(a.offset <= a.buffer_view->size, "Accessor offset exceeds buffer view");
		}
		require(cgltf_validate(parsed) == cgltf_result_success,
		        "Invalid glTF accessor, index or scene data");
		require(parsed->skins_count < 1024 && parsed->animations_count < 1024,
		        "Skin/animation count exceeds import limits");
		scene_asset scene;
		scene.source = path;
		for (cgltf_size i = 0; i < parsed->extensions_used_count; i++)
			if (std::string(parsed->extensions_used[i]) != "KHR_materials_unlit")
				scene.warnings.push_back("Optional extension not implemented: " +
				                         std::string(parsed->extensions_used[i]));
		std::string directory     = path;
		std::size_t decoded_bytes = 0;
		for (cgltf_size i = 0; i < parsed->images_count; i++) {
			auto bytes = image_bytes(parsed->images[i], directory, options, files);
			int  w, h, channels;
			require(stbi_info_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &channels) &&
			            w > 0 && h > 0 && w <= 8192 && h <= 8192,
			        "Unsupported or oversized PNG/JPEG");
			std::size_t size = std::size_t(w) * h * 4;
			require(size <= budget - decoded_bytes, "Decoded image budget exceeded");
			decoded_bytes += size;
			unsigned char *pixels =
			    stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &channels, 4);
			require(pixels, "PNG/JPEG decode failed");
			scene_image im;
			im.name =
			    parsed->images[i].name ? parsed->images[i].name : "Image " + std::to_string(i);
			im.width  = w;
			im.height = h;
			im.rgba.assign(pixels, pixels + size);
			stbi_image_free(pixels);
			scene.images.push_back(std::move(im));
		}
		for (cgltf_size i = 0; i < parsed->textures_count; i++) {
			auto &t = parsed->textures[i];
			require(t.image, "Texture has no core image (compressed formats unsupported)");
			scene_texture texture;
			texture.image = (int)(t.image - parsed->images);
			if (t.sampler) {
				auto &s = *t.sampler;
				if (s.mag_filter)
					texture.mag_filter = (int)s.mag_filter;
				if (s.min_filter)
					texture.min_filter = (int)s.min_filter;
				texture.wrap_s = (int)s.wrap_s;
				texture.wrap_t = (int)s.wrap_t;
			}
			require(texture.mag_filter == 9728 || texture.mag_filter == 9729,
			        "Invalid magnification filter");
			require(texture.min_filter == 9728 || texture.min_filter == 9729 ||
			            (texture.min_filter >= 9984 && texture.min_filter <= 9987),
			        "Invalid minification filter");
			for (int wrap : {texture.wrap_s, texture.wrap_t})
				require(wrap == 10497 || wrap == 33071 || wrap == 33648,
				        "Invalid sampler wrap mode");
			scene.textures.push_back(texture);
		}
		scene.materials.emplace_back();
		scene.materials.back().name = "Default glTF material";
		for (cgltf_size i = 0; i < parsed->materials_count; i++) {
			auto          &m = parsed->materials[i];
			scene_material material;
			material.name = m.name ? m.name : "Material " + std::to_string(i);
			std::copy(m.pbr_metallic_roughness.base_color_factor,
			          m.pbr_metallic_roughness.base_color_factor + 4, material.base_color);
			material.metallic  = m.pbr_metallic_roughness.metallic_factor;
			material.roughness = m.pbr_metallic_roughness.roughness_factor;
			std::copy(m.emissive_factor, m.emissive_factor + 3, material.emissive);
			material.alpha_mode     = m.alpha_mode == cgltf_alpha_mode_mask    ? alpha_mode::mask
			                          : m.alpha_mode == cgltf_alpha_mode_blend ? alpha_mode::blend
			                                                                   : alpha_mode::opaque;
			material.alpha_cutoff   = m.alpha_cutoff;
			material.double_sided   = m.double_sided;
			material.unlit          = m.unlit;
			material.base_color_map = slot(m.pbr_metallic_roughness.base_color_texture, *parsed);
			material.metal_rough_map =
			    slot(m.pbr_metallic_roughness.metallic_roughness_texture, *parsed);
			material.normal_map    = slot(m.normal_texture, *parsed);
			material.occlusion_map = slot(m.occlusion_texture, *parsed);
			material.emissive_map  = slot(m.emissive_texture, *parsed);
			for (float f : material.base_color)
				require(std::isfinite(f) && f >= 0 && f <= 1, "Invalid base color factor");
			for (float f : material.emissive)
				require(std::isfinite(f) && f >= 0 && f <= 1, "Invalid emissive factor");
			require(std::isfinite(material.metallic) && material.metallic >= 0 &&
			            material.metallic <= 1 && std::isfinite(material.roughness) &&
			            material.roughness >= 0 && material.roughness <= 1 &&
			            std::isfinite(material.alpha_cutoff) && material.alpha_cutoff >= 0,
			        "Invalid material scalar");
			scene.materials.push_back(material);
		}
		std::size_t vertex_bytes = decoded_bytes;
		for (cgltf_size mi = 0; mi < parsed->meshes_count; mi++) {
			auto      &mesh = parsed->meshes[mi];
			scene_mesh dst;
			dst.name = mesh.name ? mesh.name : "Mesh " + std::to_string(mi);
			for (cgltf_size pi = 0; pi < mesh.primitives_count; pi++) {
				auto &p = mesh.primitives[pi];
				require(!p.has_draco_mesh_compression && p.targets_count == 0,
				        "Draco compression and morph targets are unsupported");
				require(p.type == cgltf_primitive_type_triangles ||
				            p.type == cgltf_primitive_type_triangle_strip ||
				            p.type == cgltf_primitive_type_triangle_fan,
				        "Only triangle primitives are supported");
				const cgltf_accessor *pos = nullptr;
				for (cgltf_size a = 0; a < p.attributes_count; a++)
					if (p.attributes[a].type == cgltf_attribute_type_position)
						pos = p.attributes[a].data;
				auto            positions = unpack(pos, 3);
				scene_primitive prim;
				prim.material     = p.material ? (unsigned)(p.material - parsed->materials) + 1 : 0;
				std::size_t count = pos->count;
				require(count * sizeof(scene_vertex) <= budget - vertex_bytes,
				        "Geometry budget exceeded");
				vertex_bytes += count * sizeof(scene_vertex);
				prim.vertices.resize(count);
				for (std::size_t v = 0; v < count; v++)
					std::copy(&positions[v * 3], &positions[v * 3] + 3, prim.vertices[v].position);
				bool normals = false, have_tangents = false, uv_sets[2] = {false, false};
				bool joints = false, weights = false;
				for (cgltf_size a = 0; a < p.attributes_count; a++) {
					auto &attr = p.attributes[a];
					if (attr.type == cgltf_attribute_type_position)
						continue;
					require(attr.data->count == count, "Attribute counts disagree");
					int  components = (int)cgltf_num_components(attr.data->type);
					auto values     = unpack(attr.data, components);
					if (attr.type == cgltf_attribute_type_joints ||
					    attr.type == cgltf_attribute_type_weights) {
						require(attr.index == 0 && components == 4,
						        "Only four influences (JOINTS_0/WEIGHTS_0) are supported");
						if (attr.type == cgltf_attribute_type_joints) {
							require(!attr.data->normalized &&
							            (attr.data->component_type == cgltf_component_type_r_8u ||
							             attr.data->component_type == cgltf_component_type_r_16u),
							        "JOINTS_0 must be unsigned byte/short VEC4");
							joints = true;
							for (std::size_t v = 0; v < count; v++)
								std::copy(&values[v * 4], &values[v * 4] + 4,
								          prim.vertices[v].joints);
						} else {
							require(attr.data->component_type == cgltf_component_type_r_32f ||
							            (attr.data->normalized &&
							             (attr.data->component_type == cgltf_component_type_r_8u ||
							              attr.data->component_type == cgltf_component_type_r_16u)),
							        "WEIGHTS_0 must be float or normalized unsigned VEC4");
							weights = true;
							for (std::size_t v = 0; v < count; v++) {
								float total = 0;
								for (unsigned k = 0; k < 4; k++) {
									float w = values[v * 4 + k];
									require(w >= 0, "Negative skin weight");
									total += w;
								}
								require(std::isfinite(total) && total > 1e-8f,
								        "Invalid or zero skin weights");
								for (unsigned k = 0; k < 4; k++)
									prim.vertices[v].weights[k] = values[v * 4 + k] / total;
							}
						}
					} else if (attr.type == cgltf_attribute_type_normal) {
						require(components == 3, "NORMAL must be vec3");
						normals = true;
						for (std::size_t v = 0; v < count; v++) {
							std::copy(&values[v * 3], &values[v * 3] + 3, prim.vertices[v].normal);
							normalize(prim.vertices[v].normal);
						}
					} else if (attr.type == cgltf_attribute_type_tangent) {
						require(components == 4, "TANGENT must be vec4");
						have_tangents = true;
						for (std::size_t v = 0; v < count; v++)
							std::copy(&values[v * 4], &values[v * 4] + 4, prim.vertices[v].tangent);
					} else if (attr.type == cgltf_attribute_type_texcoord && attr.index < 2) {
						require(components == 2 && attr.index >= 0, "TEXCOORD must be vec2");
						uv_sets[attr.index] = true;
						for (std::size_t v = 0; v < count; v++)
							std::copy(&values[v * 2], &values[v * 2] + 2,
							          prim.vertices[v].uv[attr.index]);
					} else if (attr.type == cgltf_attribute_type_color && attr.index == 0) {
						require(components == 3 || components == 4, "COLOR_0 must be vec3/vec4");
						for (std::size_t v = 0; v < count; v++)
							std::copy(&values[v * components], &values[v * components] + components,
							          prim.vertices[v].color);
					} else
						scene.warnings.push_back("Ignored vertex attribute: " +
						                         std::string(attr.name ? attr.name : "unknown"));
				}
				require(joints == weights, "JOINTS_0 and WEIGHTS_0 must appear together");
				prim.skinned            = joints;
				std::size_t index_count = p.indices ? p.indices->count : count;
				require(index_count <= (budget - vertex_bytes) /
				                           (p.type == cgltf_primitive_type_triangles ? 4 : 12),
				        "Index data exceeds geometry budget");
				std::vector<std::uint32_t> indices;
				if (p.indices) {
					require(!p.indices->is_sparse && p.indices->type == cgltf_type_scalar,
					        "Unsupported index accessor");
					indices.resize(p.indices->count);
					require(cgltf_accessor_unpack_indices(p.indices, indices.data(), 4,
					                                      indices.size()) == indices.size(),
					        "Cannot unpack indices");
				} else {
					indices.resize(count);
					for (std::size_t v = 0; v < count; v++)
						indices[v] = (std::uint32_t)v;
				}
				for (auto index : indices)
					require(index < count, "Index outside vertex buffer");
				if (p.type == cgltf_primitive_type_triangles) {
					require(indices.size() % 3 == 0,
					        "Triangle index count must be divisible by three");
					prim.indices = std::move(indices);
				} else
					for (std::size_t i = 2; i < indices.size(); i++) {
						auto a = indices[p.type == cgltf_primitive_type_triangle_fan ? 0 : i - 2],
						     b = indices[i - 1], c = indices[i];
						if (p.type == cgltf_primitive_type_triangle_strip && i % 2)
							std::swap(a, b);
						if (a != b && b != c && a != c) {
							prim.indices.push_back(a);
							prim.indices.push_back(b);
							prim.indices.push_back(c);
						}
					}
				require(!prim.indices.empty(), "Primitive contains no triangles");
				vertex_bytes += prim.indices.size() * sizeof(std::uint32_t);
				const auto &material = scene.materials[prim.material];
				for (auto view :
				     {material.base_color_map, material.metal_rough_map, material.normal_map,
				      material.occlusion_map, material.emissive_map})
					if (view.texture >= 0)
						require(uv_sets[view.uv_set], "Material references a missing UV set");
				if (!normals) {
					std::vector<scene_vertex> flat;
					require(prim.indices.size() * sizeof(scene_vertex) <= budget - vertex_bytes,
					        "Flat normal expansion exceeds budget");
					vertex_bytes += prim.indices.size() * sizeof(scene_vertex);
					for (std::size_t i = 0; i < prim.indices.size(); i += 3) {
						auto va = prim.vertices[prim.indices[i]],
						     vb = prim.vertices[prim.indices[i + 1]],
						     vc = prim.vertices[prim.indices[i + 2]];
						float ab[3], ac[3], n[3];
						for (int k = 0; k < 3; k++) {
							ab[k] = vb.position[k] - va.position[k];
							ac[k] = vc.position[k] - va.position[k];
						}
						cross(ab, ac, n);
						normalize(n);
						for (auto v : {va, vb, vc}) {
							std::copy(n, n + 3, v.normal);
							flat.push_back(v);
						}
					}
					prim.vertices = std::move(flat);
					for (std::size_t i = 0; i < prim.indices.size(); i++)
						prim.indices[i] = (std::uint32_t)i;
				}
				if (!have_tangents)
					tangents(prim, material.normal_map.uv_set);
				dst.primitives.push_back((unsigned)scene.primitives.size());
				scene.primitives.push_back(std::move(prim));
			}
			scene.meshes.push_back(std::move(dst));
		}
		scene.nodes.resize(parsed->nodes_count);
		for (cgltf_size i = 0; i < parsed->nodes_count; i++) {
			auto &n = parsed->nodes[i];
			require(!n.has_mesh_gpu_instancing, "EXT_mesh_gpu_instancing unsupported");
			std::set<const cgltf_node *> ancestors;
			for (auto p = &n; p; p = p->parent)
				require(ancestors.insert(p).second, "Cycle in node hierarchy");
			auto &node            = scene.nodes[i];
			node.name             = n.name ? n.name : "Node " + std::to_string(i);
			node.parent           = n.parent ? (int)(n.parent - parsed->nodes) : -1;
			node.mesh             = n.mesh ? (int)(n.mesh - parsed->meshes) : -1;
			node.skin             = n.skin ? (int)(n.skin - parsed->skins) : -1;
			node.matrix_transform = n.has_matrix;
			if (n.has_translation)
				std::copy(n.translation, n.translation + 3, node.translation.begin());
			if (n.has_scale)
				std::copy(n.scale, n.scale + 3, node.scale.begin());
			if (n.has_rotation)
				std::copy(n.rotation, n.rotation + 4, node.rotation.begin());
			cgltf_node_transform_local(&n, glm::value_ptr(node.local));
			cgltf_node_transform_world(&n, glm::value_ptr(node.world));
			for (float f : ice::math::elements(node.world))
				require(std::isfinite(f), "Non-finite node transform");
			for (cgltf_size c = 0; c < n.children_count; c++)
				node.children.push_back((unsigned)(n.children[c] - parsed->nodes));
		}
		for (cgltf_size si = 0; si < parsed->skins_count; si++) {
			auto      &src = parsed->skins[si];
			scene_skin skin;
			skin.name     = src.name ? src.name : "Skin " + std::to_string(si);
			skin.skeleton = src.skeleton ? int(src.skeleton - parsed->nodes) : -1;
			// CPU skin data is independent of a renderer's per-draw palette capacity.
			// Unique joints cannot outnumber the already bounded hierarchy nodes.
			require(src.joints_count > 0 && src.joints_count <= parsed->nodes_count,
			        "Skin must contain between one and the scene node count of unique joints");
			std::set<unsigned> used;
			for (cgltf_size j = 0; j < src.joints_count; j++) {
				unsigned node = unsigned(src.joints[j] - parsed->nodes);
				require(used.insert(node).second, "Duplicate joint in skin");
				skin.joints.push_back(node);
				skin.inverse_bind.push_back(ice::animation::identity_matrix());
			}
			if (src.inverse_bind_matrices) {
				require(src.inverse_bind_matrices->count == src.joints_count &&
				            src.inverse_bind_matrices->component_type == cgltf_component_type_r_32f,
				        "Invalid inverse bind accessor");
				auto values = unpack(src.inverse_bind_matrices, 16);
				for (unsigned j = 0; j < skin.joints.size(); j++) {
					std::copy(&values[j * 16], &values[j * 16] + 16,
					          glm::value_ptr(skin.inverse_bind[j]));
					auto &m = skin.inverse_bind[j];
					require(ice::math::element(m, 3) == 0 && ice::math::element(m, 7) == 0 &&
					            ice::math::element(m, 11) == 0 && ice::math::element(m, 15) == 1,
					        "Inverse bind matrix must be affine");
				}
			}
			scene.skins.push_back(std::move(skin));
		}
		for (auto &node : scene.nodes)
			if (node.skin >= 0) {
				require(node.mesh >= 0, "Skinned node must have a mesh");
				for (auto index : scene.meshes[node.mesh].primitives) {
					auto &primitive = scene.primitives[index];
					require(primitive.skinned, "Skinned mesh is missing JOINTS_0/WEIGHTS_0");
					for (auto &vertex : primitive.vertices)
						for (float j : vertex.joints)
							require(j >= 0 && j < scene.skins[node.skin].joints.size() &&
							            std::floor(j) == j,
							        "Joint index outside node skin");
				}
			}
		std::size_t animation_bytes = 0;
		for (cgltf_size ai = 0; ai < parsed->animations_count; ai++) {
			auto                &src = parsed->animations[ai];
			scene_animation_clip clip;
			clip.name = src.name ? src.name : "Clip " + std::to_string(ai);
			require(src.channels_count > 0 && src.channels_count <= 10000,
			        "Empty or excessive animation channels");
			std::set<std::pair<unsigned, int>> targeted;
			bool                               first = true;
			for (cgltf_size ci = 0; ci < src.channels_count; ci++) {
				auto &channel = src.channels[ci];
				require(channel.target_node && channel.sampler, "Invalid animation target/sampler");
				auto                 &sampler = *channel.sampler;
				scene_animation_track track;
				track.node = unsigned(channel.target_node - parsed->nodes);
				require(!scene.nodes[track.node].matrix_transform,
				        "Animation cannot target a matrix node");
				require(targeted.insert({track.node, int(channel.target_path)}).second,
				        "Duplicate node/path in animation");
				unsigned components = 3;
				switch (channel.target_path) {
				case cgltf_animation_path_type_translation:
					track.path = animation_path::translation;
					break;
				case cgltf_animation_path_type_scale:
					track.path = animation_path::scale;
					break;
				case cgltf_animation_path_type_rotation:
					track.path = animation_path::rotation;
					components = 4;
					break;
				default:
					require(false, "Morph animation is not supported");
				}
				unsigned multiplier = 1;
				switch (sampler.interpolation) {
				case cgltf_interpolation_type_linear:
					track.interpolation = animation_interpolation::linear;
					break;
				case cgltf_interpolation_type_step:
					track.interpolation = animation_interpolation::step;
					break;
				case cgltf_interpolation_type_cubic_spline:
					track.interpolation = animation_interpolation::cubic_spline;
					multiplier          = 3;
					break;
				default:
					require(false, "Unsupported animation interpolation");
				}
				require(sampler.input && sampler.output &&
				            sampler.input->component_type == cgltf_component_type_r_32f &&
				            sampler.output->component_type == cgltf_component_type_r_32f,
				        "Animation accessors must use float data");
				require((multiplier != 3 || sampler.input->count >= 2) &&
				            sampler.input->count <= 2000000 &&
				            sampler.output->count == sampler.input->count * multiplier,
				        "Animation key counts disagree");
				auto bytes = sampler.input->count * sizeof(float) * (1 + components * multiplier);
				require(bytes <= budget - animation_bytes, "Animation data budget exceeded");
				animation_bytes += bytes;
				track.times  = unpack(sampler.input, 1);
				track.values = unpack(sampler.output, components);
				for (unsigned k = 0; k < track.times.size(); k++)
					require(track.times[k] >= 0 && (k == 0 || track.times[k] > track.times[k - 1]),
					        "Animation times must be nonnegative and strictly increasing");
				if (components == 4)
					for (unsigned k = 0; k < track.times.size(); k++) {
						unsigned offset = (k * multiplier + (multiplier == 3 ? 1 : 0)) * 4;
						double   norm   = 0;
						for (unsigned c = 0; c < 4; c++)
							norm += double(track.values[offset + c]) * track.values[offset + c];
						require(norm > 1e-12, "Zero animation quaternion");
					}
				clip.start =
				    first ? track.times.front() : std::min(clip.start, track.times.front());
				clip.end = first ? track.times.back() : std::max(clip.end, track.times.back());
				first    = false;
				clip.tracks.push_back(std::move(track));
			}
			scene.animations.push_back(std::move(clip));
		}
		cgltf_scene *selected =
		    parsed->scene ? parsed->scene : (parsed->scenes_count ? &parsed->scenes[0] : nullptr);
		if (selected)
			for (cgltf_size i = 0; i < selected->nodes_count; i++)
				scene.roots.push_back((unsigned)(selected->nodes[i] - parsed->nodes));
		else
			for (unsigned i = 0; i < scene.nodes.size(); i++)
				if (scene.nodes[i].parent < 0)
					scene.roots.push_back(i);
		bool                  bounds  = false;
		std::vector<unsigned> pending = scene.roots;
		std::set<unsigned>    visited;
		while (!pending.empty()) {
			auto i = pending.back();
			pending.pop_back();
			require(visited.insert(i).second, "Scene node is referenced twice");
			auto &node = scene.nodes[i];
			pending.insert(pending.end(), node.children.begin(), node.children.end());
			if (node.mesh < 0)
				continue;
			for (auto primitive : scene.meshes[node.mesh].primitives) {
				scene_instance instance;
				instance.node      = i;
				instance.primitive = primitive;
				scene.instances.push_back(instance);
				for (auto &v : scene.primitives[primitive].vertices) {
					float p[3];
					for (int k = 0; k < 3; k++)
						p[k] = ice::math::element(node.world, k) * v.position[0] +
						       ice::math::element(node.world, 4 + k) * v.position[1] +
						       ice::math::element(node.world, 8 + k) * v.position[2] +
						       ice::math::element(node.world, 12 + k);
					for (float f : p)
						require(std::isfinite(f), "Non-finite transformed vertex");
					if (!bounds) {
						std::copy(p, p + 3, scene.bounds_min);
						std::copy(p, p + 3, scene.bounds_max);
						bounds = true;
					} else
						for (int k = 0; k < 3; k++) {
							scene.bounds_min[k] = std::min(scene.bounds_min[k], p[k]);
							scene.bounds_max[k] = std::max(scene.bounds_max[k], p[k]);
						}
				}
			}
		}
		require(!scene.instances.empty(), "Selected scene has no renderable mesh instances");
		ice::animation::pose rest;
		std::string          pose_error;
		bool valid_pose = ice::animation::evaluate_pose(scene, -1, 0, false, rest, pose_error);
		require(valid_pose, "Invalid rest pose: " + pose_error);
		bool valid_bounds = ice::animation::scene_pose_bounds(scene, rest, scene.bounds_min,
		                                                      scene.bounds_max, pose_error);
		require(valid_bounds, "Invalid posed bounds: " + pose_error);
		scene.dependencies = files.paths;
		output             = std::move(scene);
		return true;
	} catch (const std::exception &e) {
		diagnostics = e.what();
		return false;
	}
}

} // namespace ice::assets
