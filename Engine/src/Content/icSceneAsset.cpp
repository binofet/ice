#include "Content/icSceneAsset.h"
#include "cgltf/cgltf.h"
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

namespace {
const std::size_t Budget = 256u * 1024u * 1024u;
void require(bool value, const std::string &why) {
    if (!value)
        throw std::runtime_error(why);
}
struct Alloc {
    std::size_t used = 0;
};
struct alignas(std::max_align_t) Block {
    std::size_t size;
};
void *allocate(void *u, cgltf_size n) {
    auto &a = *static_cast<Alloc *>(u);
    if (n > Budget - a.used)
        return nullptr;
    auto b = static_cast<Block *>(std::malloc(sizeof(Block) + n));
    if (!b)
        return nullptr;
    b->size = n;
    a.used += n;
    return b + 1;
}
void release(void *u, void *p) {
    if (!p)
        return;
    auto b = static_cast<Block *>(p) - 1;
    static_cast<Alloc *>(u)->used -= b->size;
    std::free(b);
}
struct Files {
    std::vector<std::string> paths;
    std::size_t bytes = 0;
    icResourceResolver *resolver = nullptr;
    std::string error;
};
cgltf_result readFile(const cgltf_memory_options *mem, const cgltf_file_options *opt,
                      const char *path, cgltf_size *size, void **data) {
    auto &f = *static_cast<Files *>(opt->user_data);
    std::vector<unsigned char> resource;
    if (!f.resolver->Read(path, resource, f.error))
        return cgltf_result_io_error;
    if (resource.size() > Budget - f.bytes)
        return cgltf_result_out_of_memory;
    cgltf_size actual = resource.size();
    if (*size && *size > actual)
        return cgltf_result_data_too_short;
    cgltf_size count = *size ? *size : actual;
    void *bytes = mem->alloc_func(mem->user_data, count);
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
void releaseFile(const cgltf_memory_options *mem, const cgltf_file_options *, void *data,
                 cgltf_size) {
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
std::vector<unsigned char> imageBytes(const cgltf_image &image, const std::string &directory,
                                      const cgltf_options &opt, Files &files) {
    if (image.buffer_view) {
        auto view = image.buffer_view;
        auto bytes = cgltf_buffer_view_data(view);
        require(bytes, "Missing image buffer view");
        return {bytes, bytes + view->size};
    }
    require(image.uri, "Image has no source");
    std::string uri = image.uri;
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
        require(n < Budget && cgltf_load_buffer_base64(&opt, n, uri.c_str() + comma + 1, &bytes) ==
                                  cgltf_result_success,
                "Invalid base64 image");
        std::vector<unsigned char> out((unsigned char *)bytes, (unsigned char *)bytes + n);
        opt.memory.free_func(opt.memory.user_data, bytes);
        return out;
    }
    icResourceURI base, resolved;
    std::string error;
    require(icResourceURI::Parse(directory, base, error) &&
                icResourceURI::Resolve(base, uri, resolved, error),
            "Invalid image URI: " + error);
    cgltf_size size = 0;
    void *data = nullptr;
    auto readResult = readFile(&opt.memory, &opt.file, resolved.Document().c_str(), &size, &data);
    require(readResult == cgltf_result_success,
            "Cannot read image: " + resolved.String() + " " + files.error);
    std::vector<unsigned char> out((unsigned char *)data, (unsigned char *)data + size);
    releaseFile(&opt.memory, &opt.file, data, size);
    return out;
}
icSceneTextureSlot slot(const cgltf_texture_view &v, const cgltf_data &data) {
    icSceneTextureSlot s;
    if (!v.texture)
        return s;
    require(!v.has_transform, "KHR_texture_transform is not supported yet");
    require(v.texcoord >= 0 && v.texcoord <= 1, "Only TEXCOORD_0 and TEXCOORD_1 are supported");
    require(std::isfinite(v.scale), "Non-finite texture scale");
    s.texture = (int)(v.texture - data.textures);
    s.uvSet = v.texcoord;
    s.scale = v.scale;
    return s;
}
void tangents(icScenePrimitive &primitive, int uvSet) {
    std::vector<std::array<float, 3>> tan(primitive.vertices.size()),
        bitan(primitive.vertices.size());
    for (std::size_t i = 0; i < primitive.indices.size(); i += 3) {
        auto a = primitive.indices[i], b = primitive.indices[i + 1], c = primitive.indices[i + 2];
        auto &va = primitive.vertices[a];
        auto &vb = primitive.vertices[b];
        auto &vc = primitive.vertices[c];
        float e1[3], e2[3];
        for (int k = 0; k < 3; k++) {
            e1[k] = vb.position[k] - va.position[k];
            e2[k] = vc.position[k] - va.position[k];
        }
        float du1 = vb.uv[uvSet][0] - va.uv[uvSet][0], dv1 = vb.uv[uvSet][1] - va.uv[uvSet][1],
              du2 = vc.uv[uvSet][0] - va.uv[uvSet][0], dv2 = vc.uv[uvSet][1] - va.uv[uvSet][1],
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
        auto &v = primitive.vertices[i];
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
        v.tangent[3] = dot(b, bitan[i].data()) < 0 ? -1 : 1;
    }
}
} // namespace

bool icLoadGLTF(const std::string &path, icSceneAsset &output, std::string &diagnostics) {
    icResourceResolver resolver;
    return icLoadGLTF(path, resolver, output, diagnostics);
}
bool icLoadGLTF(const std::string &input, icResourceResolver &resolver, icSceneAsset &output,
                std::string &diagnostics) {
    diagnostics.clear();
    icResourceURI source;
    if (!resolver.Canonical(input, source, diagnostics))
        return false;
    const std::string path = source.Document();
    Alloc memory;
    Files files;
    files.resolver = &resolver;
    cgltf_options options{};
    options.memory = {allocate, release, &memory};
    options.file = {readFile, releaseFile, &files};
    cgltf_data *parsed = nullptr;
    try {
        auto parseResult = cgltf_parse_file(&options, path.c_str(), &parsed);
        require(parseResult == cgltf_result_success,
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
            std::string uri = buffer.uri;
            if (uri.compare(0, 5, "data:") == 0) {
                auto comma = uri.find(',');
                require(comma != std::string::npos &&
                            uri.substr(0, comma).find(";base64") != std::string::npos &&
                            buffer.size <= (uri.size() - comma - 1) / 4 * 3,
                        "Invalid buffer data URI");
                require(cgltf_load_buffer_base64(&options, buffer.size, uri.c_str() + comma + 1,
                                                 &buffer.data) == cgltf_result_success,
                        "Cannot decode buffer data URI");
                buffer.data_free_method = cgltf_data_free_method_memory_free;
            } else {
                icResourceURI resolved;
                std::string error;
                require(icResourceURI::Resolve(source, uri, resolved, error),
                        "Invalid buffer URI: " + error);
                cgltf_size size = buffer.size;
                auto readResult = readFile(&options.memory, &options.file,
                                           resolved.Document().c_str(), &size, &buffer.data);
                require(readResult == cgltf_result_success,
                        "Cannot read buffer " + resolved.String() + " " + files.error);
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
        require(parsed->skins_count == 0 && parsed->animations_count == 0,
                "Skins and animations are not supported in the static viewer");
        icSceneAsset scene;
        scene.source = path;
        for (cgltf_size i = 0; i < parsed->extensions_used_count; i++)
            if (std::string(parsed->extensions_used[i]) != "KHR_materials_unlit")
                scene.warnings.push_back("Optional extension not implemented: " +
                                         std::string(parsed->extensions_used[i]));
        std::string directory = path;
        std::size_t decodedBytes = 0;
        for (cgltf_size i = 0; i < parsed->images_count; i++) {
            auto bytes = imageBytes(parsed->images[i], directory, options, files);
            int w, h, channels;
            require(stbi_info_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &channels) &&
                        w > 0 && h > 0 && w <= 8192 && h <= 8192,
                    "Unsupported or oversized PNG/JPEG");
            std::size_t size = std::size_t(w) * h * 4;
            require(size <= Budget - decodedBytes, "Decoded image budget exceeded");
            decodedBytes += size;
            unsigned char *pixels =
                stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &channels, 4);
            require(pixels, "PNG/JPEG decode failed");
            icSceneImage im;
            im.name =
                parsed->images[i].name ? parsed->images[i].name : "Image " + std::to_string(i);
            im.width = w;
            im.height = h;
            im.rgba.assign(pixels, pixels + size);
            stbi_image_free(pixels);
            scene.images.push_back(std::move(im));
        }
        for (cgltf_size i = 0; i < parsed->textures_count; i++) {
            auto &t = parsed->textures[i];
            require(t.image, "Texture has no core image (compressed formats unsupported)");
            icSceneTexture texture;
            texture.image = (int)(t.image - parsed->images);
            if (t.sampler) {
                auto &s = *t.sampler;
                if (s.mag_filter)
                    texture.magFilter = (int)s.mag_filter;
                if (s.min_filter)
                    texture.minFilter = (int)s.min_filter;
                texture.wrapS = (int)s.wrap_s;
                texture.wrapT = (int)s.wrap_t;
            }
            require(texture.magFilter == 9728 || texture.magFilter == 9729,
                    "Invalid magnification filter");
            require(texture.minFilter == 9728 || texture.minFilter == 9729 ||
                        (texture.minFilter >= 9984 && texture.minFilter <= 9987),
                    "Invalid minification filter");
            for (int wrap : {texture.wrapS, texture.wrapT})
                require(wrap == 10497 || wrap == 33071 || wrap == 33648,
                        "Invalid sampler wrap mode");
            scene.textures.push_back(texture);
        }
        scene.materials.emplace_back();
        scene.materials.back().name = "Default glTF material";
        for (cgltf_size i = 0; i < parsed->materials_count; i++) {
            auto &m = parsed->materials[i];
            icSceneMaterial material;
            material.name = m.name ? m.name : "Material " + std::to_string(i);
            std::copy(m.pbr_metallic_roughness.base_color_factor,
                      m.pbr_metallic_roughness.base_color_factor + 4, material.baseColor);
            material.metallic = m.pbr_metallic_roughness.metallic_factor;
            material.roughness = m.pbr_metallic_roughness.roughness_factor;
            std::copy(m.emissive_factor, m.emissive_factor + 3, material.emissive);
            material.alphaMode = m.alpha_mode == cgltf_alpha_mode_mask    ? icAlphaMode::Mask
                                 : m.alpha_mode == cgltf_alpha_mode_blend ? icAlphaMode::Blend
                                                                          : icAlphaMode::Opaque;
            material.alphaCutoff = m.alpha_cutoff;
            material.doubleSided = m.double_sided;
            material.unlit = m.unlit;
            material.baseColorMap = slot(m.pbr_metallic_roughness.base_color_texture, *parsed);
            material.metalRoughMap =
                slot(m.pbr_metallic_roughness.metallic_roughness_texture, *parsed);
            material.normalMap = slot(m.normal_texture, *parsed);
            material.occlusionMap = slot(m.occlusion_texture, *parsed);
            material.emissiveMap = slot(m.emissive_texture, *parsed);
            for (float f : material.baseColor)
                require(std::isfinite(f) && f >= 0 && f <= 1, "Invalid base color factor");
            for (float f : material.emissive)
                require(std::isfinite(f) && f >= 0 && f <= 1, "Invalid emissive factor");
            require(std::isfinite(material.metallic) && material.metallic >= 0 &&
                        material.metallic <= 1 && std::isfinite(material.roughness) &&
                        material.roughness >= 0 && material.roughness <= 1 &&
                        std::isfinite(material.alphaCutoff) && material.alphaCutoff >= 0,
                    "Invalid material scalar");
            scene.materials.push_back(material);
        }
        std::size_t vertexBytes = decodedBytes;
        for (cgltf_size mi = 0; mi < parsed->meshes_count; mi++) {
            auto &mesh = parsed->meshes[mi];
            icSceneMesh dst;
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
                auto positions = unpack(pos, 3);
                icScenePrimitive prim;
                prim.material = p.material ? (unsigned)(p.material - parsed->materials) + 1 : 0;
                std::size_t count = pos->count;
                require(count * sizeof(icSceneVertex) <= Budget - vertexBytes,
                        "Geometry budget exceeded");
                vertexBytes += count * sizeof(icSceneVertex);
                prim.vertices.resize(count);
                for (std::size_t v = 0; v < count; v++)
                    std::copy(&positions[v * 3], &positions[v * 3] + 3, prim.vertices[v].position);
                bool normals = false, haveTangents = false, uvSets[2] = {false, false};
                for (cgltf_size a = 0; a < p.attributes_count; a++) {
                    auto &attr = p.attributes[a];
                    if (attr.type == cgltf_attribute_type_position)
                        continue;
                    require(attr.data->count == count, "Attribute counts disagree");
                    int components = (int)cgltf_num_components(attr.data->type);
                    auto values = unpack(attr.data, components);
                    if (attr.type == cgltf_attribute_type_normal) {
                        require(components == 3, "NORMAL must be vec3");
                        normals = true;
                        for (std::size_t v = 0; v < count; v++) {
                            std::copy(&values[v * 3], &values[v * 3] + 3, prim.vertices[v].normal);
                            normalize(prim.vertices[v].normal);
                        }
                    } else if (attr.type == cgltf_attribute_type_tangent) {
                        require(components == 4, "TANGENT must be vec4");
                        haveTangents = true;
                        for (std::size_t v = 0; v < count; v++)
                            std::copy(&values[v * 4], &values[v * 4] + 4, prim.vertices[v].tangent);
                    } else if (attr.type == cgltf_attribute_type_texcoord && attr.index < 2) {
                        require(components == 2 && attr.index >= 0, "TEXCOORD must be vec2");
                        uvSets[attr.index] = true;
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
                std::size_t indexCount = p.indices ? p.indices->count : count;
                require(indexCount <= (Budget - vertexBytes) /
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
                vertexBytes += prim.indices.size() * sizeof(std::uint32_t);
                const auto &material = scene.materials[prim.material];
                for (auto view : {material.baseColorMap, material.metalRoughMap, material.normalMap,
                                  material.occlusionMap, material.emissiveMap})
                    if (view.texture >= 0)
                        require(uvSets[view.uvSet], "Material references a missing UV set");
                if (!normals) {
                    std::vector<icSceneVertex> flat;
                    require(prim.indices.size() * sizeof(icSceneVertex) <= Budget - vertexBytes,
                            "Flat normal expansion exceeds budget");
                    vertexBytes += prim.indices.size() * sizeof(icSceneVertex);
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
                if (!haveTangents)
                    tangents(prim, material.normalMap.uvSet);
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
            auto &node = scene.nodes[i];
            node.name = n.name ? n.name : "Node " + std::to_string(i);
            node.parent = n.parent ? (int)(n.parent - parsed->nodes) : -1;
            node.mesh = n.mesh ? (int)(n.mesh - parsed->meshes) : -1;
            cgltf_node_transform_local(&n, node.local.data());
            cgltf_node_transform_world(&n, node.world.data());
            for (float f : node.world)
                require(std::isfinite(f), "Non-finite node transform");
            for (cgltf_size c = 0; c < n.children_count; c++)
                node.children.push_back((unsigned)(n.children[c] - parsed->nodes));
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
        bool bounds = false;
        std::vector<unsigned> pending = scene.roots;
        std::set<unsigned> visited;
        while (!pending.empty()) {
            auto i = pending.back();
            pending.pop_back();
            require(visited.insert(i).second, "Scene node is referenced twice");
            auto &node = scene.nodes[i];
            pending.insert(pending.end(), node.children.begin(), node.children.end());
            if (node.mesh < 0)
                continue;
            for (auto primitive : scene.meshes[node.mesh].primitives) {
                icSceneInstance instance;
                instance.node = i;
                instance.primitive = primitive;
                scene.instances.push_back(instance);
                for (auto &v : scene.primitives[primitive].vertices) {
                    float p[3];
                    for (int k = 0; k < 3; k++)
                        p[k] = node.world[k] * v.position[0] + node.world[4 + k] * v.position[1] +
                               node.world[8 + k] * v.position[2] + node.world[12 + k];
                    for (float f : p)
                        require(std::isfinite(f), "Non-finite transformed vertex");
                    if (!bounds) {
                        std::copy(p, p + 3, scene.boundsMin);
                        std::copy(p, p + 3, scene.boundsMax);
                        bounds = true;
                    } else
                        for (int k = 0; k < 3; k++) {
                            scene.boundsMin[k] = std::min(scene.boundsMin[k], p[k]);
                            scene.boundsMax[k] = std::max(scene.boundsMax[k], p[k]);
                        }
                }
            }
        }
        require(!scene.instances.empty(), "Selected scene has no renderable mesh instances");
        scene.dependencies = files.paths;
        output = std::move(scene);
        return true;
    } catch (const std::exception &e) {
        diagnostics = e.what();
        return false;
    }
}
