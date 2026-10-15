#include "ice/render/gl/pipeline.h"
#include "ice/render/gl/render_target.h"
#include "ice/render/scene_renderer.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <set>
#include <vector>
namespace ice::render::gl {

namespace {
struct mesh_buffers {
	GLuint   vertices = 0, indices = 0;
	GLsizei  count    = 0;
	unsigned material = 0;
};
struct instance {
	ice::math::mat4 world;
	unsigned        primitive = 0, node = 0;
	int             skin            = -1;
	float           local_center[3] = {};
	float           center[3]       = {};
	float           determinant     = 1;
};
float determinant(const float *m) {
	return m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) +
	       m[8] * (m[1] * m[6] - m[5] * m[2]);
}
struct resources {
	std::vector<mesh_buffers>                  meshes;
	std::vector<GLuint>                        textures;
	std::vector<ice::assets::scene_material>   materials;
	std::vector<instance>                      instances;
	ice::animation::pose                       rest;
	std::vector<std::pair<unsigned, unsigned>> bones;
	std::vector<unsigned>                      joint_nodes;
	~resources() {
		for (auto &m : meshes) {
			if (m.vertices)
				glDeleteBuffers(1, &m.vertices);
			if (m.indices)
				glDeleteBuffers(1, &m.indices);
		}
		if (!textures.empty())
			glDeleteTextures((GLsizei)textures.size(), textures.data());
	}
};
class scene_gl final : public ice::render::scene_renderer {
public:
	explicit scene_gl(std::shared_ptr<ice::io::resource_resolver> r) : resolver_(std::move(r)) {}
	bool upload(const ice::assets::scene_asset &scene, std::string &diagnostics) override;
	bool draw(const ice::render::scene_view &view, std::string &diagnostics) override;
	void release() override {
		target_.release();
		composite_.release();
		resources_.reset();
		pipelines_.clear();
		vertex_.reset();
		fragment_.reset();
		skin_vertex_.reset();
	}
	~scene_gl() { release(); }

private:
	std::shared_ptr<ice::io::resource_resolver> resolver_;
	bool shader_text(const char *uri, std::string &text, std::string &diagnostics) {
		std::vector<unsigned char> bytes;
		if (!resolver_ || !resolver_->read(uri, bytes, diagnostics)) {
			if (diagnostics.empty())
				diagnostics = "Shader URI resolver is required";
			return false;
		}
		if (bytes.size() > 1024 * 1024) {
			diagnostics = "Shader source exceeds 1 MiB";
			return false;
		}
		text.assign(bytes.begin(), bytes.end());
		return true;
	}
	bool                                          framebuffer_srgb_ = false;
	render_target                                 target_;
	program                                       composite_;
	std::unique_ptr<resources>                    resources_;
	std::shared_ptr<shader>                       vertex_, fragment_, skin_vertex_;
	std::map<unsigned, std::unique_ptr<pipeline>> pipelines_;
	ice::render::gl::pipeline *pipeline_for_material(const ice::assets::scene_material &material,
	                                                 bool mirror, bool wire, bool skinned,
	                                                 std::string &diagnostics);
};
pipeline *scene_gl::pipeline_for_material(const ice::assets::scene_material &material, bool mirror,
                                          bool wire, bool skinned, std::string &diagnostics) {
	bool     blend = material.alpha_mode == ice::assets::alpha_mode::blend;
	unsigned key   = (blend ? 1 : 0) | (material.double_sided ? 2 : 0) | (mirror ? 4 : 0) |
	               (wire ? 8 : 0) | (skinned ? 16 : 0);
	auto found = pipelines_.find(key);
	if (found != pipelines_.end())
		return found->second.get();
	ice::render::pipeline_desc desc;
	desc.vertex              = skinned ? skin_vertex_ : vertex_;
	desc.fragment            = fragment_;
	desc.label               = "glTF metallic/roughness";
	desc.color_target        = ice::render::color_target_format::linear_rgba16_float;
	desc.vertices.stride     = sizeof(ice::assets::scene_vertex);
	desc.vertices.attributes = {
	    {ice::render::vertex_semantic::position, ice::render::vertex_format::float3,
	     offsetof(ice::assets::scene_vertex, position)},
	    {ice::render::vertex_semantic::normal, ice::render::vertex_format::float3,
	     offsetof(ice::assets::scene_vertex, normal)},
	    {ice::render::vertex_semantic::color0, ice::render::vertex_format::float4,
	     offsetof(ice::assets::scene_vertex, color)},
	    {ice::render::vertex_semantic::tex_coord0, ice::render::vertex_format::float2,
	     offsetof(ice::assets::scene_vertex, uv)},
	    {ice::render::vertex_semantic::tex_coord1, ice::render::vertex_format::float2,
	     offsetof(ice::assets::scene_vertex, uv) + sizeof(float) * 2},
	    {ice::render::vertex_semantic::tangent, ice::render::vertex_format::float4,
	     offsetof(ice::assets::scene_vertex, tangent)},
	    {ice::render::vertex_semantic::joints0, ice::render::vertex_format::float4,
	     offsetof(ice::assets::scene_vertex, joints)},
	    {ice::render::vertex_semantic::weights0, ice::render::vertex_format::float4,
	     offsetof(ice::assets::scene_vertex, weights)}};
	desc.state.blend = blend ? ice::render::blend_mode::alpha : ice::render::blend_mode::opaque;
	desc.state.depth_write = !blend;
	desc.state.cull =
	    material.double_sided ? ice::render::cull_mode::none : ice::render::cull_mode::back;
	desc.state.clockwise_front = mirror;
	desc.state.wireframe       = wire;
	auto pipeline              = std::make_unique<ice::render::gl::pipeline>();
	if (!pipeline->create(desc, diagnostics))
		return nullptr;
	auto pointer    = pipeline.get();
	pipelines_[key] = std::move(pipeline);
	return pointer;
}
bool scene_gl::upload(const ice::assets::scene_asset &scene, std::string &diagnostics) {
	diagnostics.clear();
	if (!vertex_) {
		auto                       v = std::make_shared<shader>(), f = std::make_shared<shader>();
		ice::render::shader_source source;
		source.label = "glTF vertex";
		source.uri   = "shaders://project/gltf.vert";
		if (!shader_text(source.uri.c_str(), source.source, diagnostics) ||
		    !v->compile(source, diagnostics))
			return false;
		source.stage = ice::render::shader_stage::fragment;
		source.label = "glTF PBR";
		source.uri   = "shaders://project/gltf.frag";
		if (!shader_text(source.uri.c_str(), source.source, diagnostics) ||
		    !f->compile(source, diagnostics))
			return false;
		vertex_   = v;
		fragment_ = f;
	}
	for (auto &skin : scene.skins)
		if (skin.joints.size() > 64) {
			diagnostics = "GL compatibility skinning supports at most 64 joints per skin";
			return false;
		}
	if (!scene.skins.empty() && !skin_vertex_) {
		GLint components = 0;
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &components);
		if (components < 64 * 16 + 128) {
			diagnostics = "GPU has insufficient vertex uniforms for 64-joint skinning";
			return false;
		}
		auto stage   = std::make_shared<shader>();
		auto source  = vertex_->source();
		auto version = source.source.find("#version");
		auto newline = source.source.find('\n', version);
		if (version == std::string::npos || newline == std::string::npos) {
			diagnostics = "Skinning shader needs a version line";
			return false;
		}
		source.source.insert(newline + 1, "#define ICE_SKINNED\n");
		source.label = "glTF skinned vertex";
		if (!stage->compile(source, diagnostics))
			return false;
		skin_vertex_ = stage;
	}
	const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
	framebuffer_srgb_      = extensions && (std::strstr(extensions, "GL_EXT_framebuffer_sRGB") ||
                                       std::strstr(extensions, "GL_ARB_framebuffer_sRGB"));
	GLint units            = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &units);
	if (units < 5) {
		diagnostics = "glTF PBR requires five fragment texture units";
		return false;
	}
	if (!composite_.valid()) {
		std::string v, f;
		if (!shader_text("shaders://project/fullscreen.vert", v, diagnostics) ||
		    !shader_text("shaders://project/display.frag", f, diagnostics) ||
		    !composite_.compile(v.c_str(), f.c_str(), diagnostics))
			return false;
	}

	auto next       = std::unique_ptr<resources>(new resources);
	next->materials = scene.materials;
	if (!ice::animation::evaluate_pose(scene, -1, 0, false, next->rest, diagnostics))
		return false;
	std::set<unsigned>                      joint_nodes;
	std::set<std::pair<unsigned, unsigned>> bones;
	for (auto &skin : scene.skins) {
		std::set<unsigned> members(skin.joints.begin(), skin.joints.end());
		for (auto joint : skin.joints) {
			joint_nodes.insert(joint);
			int parent = scene.nodes[joint].parent;
			while (parent >= 0 && !members.count(unsigned(parent)))
				parent = scene.nodes[parent].parent;
			if (parent >= 0)
				bones.insert({joint, unsigned(parent)});
		}
	}
	next->bones.assign(bones.begin(), bones.end());
	next->joint_nodes.assign(joint_nodes.begin(), joint_nodes.end());
	GLint old_active = 0, old_vbo = 0, old_ibo = 0;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_vbo);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &old_ibo);
	glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT);
	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glActiveTexture(GL_TEXTURE0);
	for (auto &primitive : scene.primitives) {
		mesh_buffers mesh;
		mesh.count    = (GLsizei)primitive.indices.size();
		mesh.material = primitive.material;
		glGenBuffers(1, &mesh.vertices);
		glGenBuffers(1, &mesh.indices);
		next->meshes.push_back(mesh);
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
		glBufferData(GL_ARRAY_BUFFER, primitive.vertices.size() * sizeof(ice::assets::scene_vertex),
		             primitive.vertices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, primitive.indices.size() * sizeof(std::uint32_t),
		             primitive.indices.data(), GL_STATIC_DRAW);
	}
	// A glTF texture owns sampler state independently even when images are shared.
	for (auto &texture : scene.textures) {
		GLuint name = 0;
		glGenTextures(1, &name);
		next->textures.push_back(name);
		glBindTexture(GL_TEXTURE_2D, name);
		auto &image = scene.images[texture.image];
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture.mag_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture.min_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texture.wrap_s);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texture.wrap_t);
		bool mip = texture.min_filter != GL_NEAREST && texture.min_filter != GL_LINEAR;
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mip ? GL_TRUE : GL_FALSE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width, image.height, 0, GL_RGBA,
		             GL_UNSIGNED_BYTE, image.rgba.data());
	}
	// White color/data defaults, then a flat normal map (no global image flipping).
	for (int i = 0; i < 2; i++) {
		GLuint name = 0;
		glGenTextures(1, &name);
		next->textures.push_back(name);
		glBindTexture(GL_TEXTURE_2D, name);
		unsigned char pixel[4] = {255, 255, 255, 255};
		if (i == 1) {
			pixel[0] = 128;
			pixel[1] = 128;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	}
	glBindBuffer(GL_ARRAY_BUFFER, old_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, old_ibo);
	glPopClientAttrib();
	glPopAttrib();
	glActiveTexture(old_active);
	if (glGetError() != GL_NO_ERROR) {
		diagnostics = "GL mesh/texture upload failed; previous scene retained";
		return false;
	}
	for (auto &item : scene.instances) {
		instance instance;
		instance.world       = scene.nodes[item.node].world;
		instance.primitive   = item.primitive;
		instance.node        = item.node;
		instance.skin        = scene.nodes[item.node].skin;
		instance.determinant = determinant(glm::value_ptr(instance.world));
		float center[3]      = {};
		auto &vertices       = scene.primitives[item.primitive].vertices;
		for (auto &v : vertices)
			for (int k = 0; k < 3; k++)
				center[k] += v.position[k] / vertices.size();
		std::copy(center, center + 3, instance.local_center);
		for (int k = 0; k < 3; k++)
			instance.center[k] = ice::math::element(instance.world, k) * center[0] +
			                     ice::math::element(instance.world, k + 4) * center[1] +
			                     ice::math::element(instance.world, k + 8) * center[2] +
			                     ice::math::element(instance.world, k + 12);
		next->instances.push_back(instance);
	}
	for (auto &m : scene.materials) {
		if (!pipeline_for_material(m, false, false, false, diagnostics))
			return false;
		if (!scene.skins.empty() && !pipeline_for_material(m, false, false, true, diagnostics))
			return false;
	}
	resources_ = std::move(next);
	return true;
}
bool scene_gl::draw(const ice::render::scene_view &view, std::string &diagnostics) {
	diagnostics.clear();
	if (!resources_) {
		diagnostics = "No uploaded scene";
		return false;
	}
	const auto &pose = view.pose ? *view.pose : resources_->rest;
	if (pose.world.size() != resources_->rest.world.size() ||
	    pose.palettes.size() != resources_->rest.palettes.size()) {
		diagnostics = "Pose does not match uploaded scene";
		return false;
	}
	for (unsigned i = 0; i < pose.palettes.size(); i++)
		if (pose.palettes[i].size() != resources_->rest.palettes[i].size()) {
			diagnostics = "Pose skin palette does not match uploaded scene";
			return false;
		}
	auto instances = resources_->instances;
	for (auto &instance : instances) {
		instance.world       = pose.world[instance.node];
		instance.determinant = determinant(glm::value_ptr(instance.world));
		for (unsigned k = 0; k < 3; k++)
			instance.center[k] =
			    ice::math::element(instance.world, k) * instance.local_center[0] +
			    ice::math::element(instance.world, k + 4) * instance.local_center[1] +
			    ice::math::element(instance.world, k + 8) * instance.local_center[2] +
			    ice::math::element(instance.world, k + 12);
		if (instance.skin >= 0) {
			auto &palette        = pose.palettes[instance.skin];
			instance.determinant = determinant(glm::value_ptr(palette[0]));
			for (unsigned k = 0; k < 3; k++) {
				instance.center[k] = 0;
				for (auto &joint : palette)
					instance.center[k] += ice::math::element(joint, k + 12) / palette.size();
			}
		}
	}
	if (view.width <= 0 || view.height <= 0)
		return true;
	if (!target_.resize(view.width, view.height, diagnostics))
		return false;
	GLuint previous_framebuffer = render_target::current_framebuffer();
	GLint  previous_program = 0, matrix_mode = 0, active = 0, client_active = 0, old_vbo = 0,
	      old_ibo = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
	glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &client_active);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_vbo);
	glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &old_ibo);
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_FOG);
	glDisable(GL_SCISSOR_TEST);
	glViewport(0, 0, view.width, view.height);
	if (framebuffer_srgb_)
		glDisable(0x8DB9);
	target_.bind();
	glDrawBuffer(0x8CE0);
	glReadBuffer(0x8CE0);
	glDepthMask(GL_TRUE);
	glClearColor(.018f, .025f, .04f, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(glm::value_ptr(view.projection));
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	std::vector<const instance *> order;
	for (auto &instance : instances)
		if (std::fabs(instance.determinant) > 1e-12f &&
		    (view.material < 0 ||
		     (int)resources_->meshes[instance.primitive].material == view.material))
			order.push_back(&instance);
	auto depth = [&](const instance *i) {
		return ice::math::element(view.view, 2) * i->center[0] +
		       ice::math::element(view.view, 6) * i->center[1] +
		       ice::math::element(view.view, 10) * i->center[2] + ice::math::element(view.view, 14);
	};
	std::stable_sort(order.begin(), order.end(), [&](const instance *a, const instance *b) {
		bool aa = resources_->materials[resources_->meshes[a->primitive].material].alpha_mode ==
		          ice::assets::alpha_mode::blend,
		     bb = resources_->materials[resources_->meshes[b->primitive].material].alpha_mode ==
		          ice::assets::alpha_mode::blend;
		if (aa != bb)
			return !aa;
		return aa ? depth(a) < depth(b) : false;
	});
	bool ok = true;
	for (auto instance : order) {
		auto &mesh     = resources_->meshes[instance->primitive];
		auto &material = resources_->materials[mesh.material];
		auto  pipeline = this->pipeline_for_material(
            material, instance->determinant < 0, view.wireframe, instance->skin >= 0, diagnostics);
		if (!pipeline) {
			ok = false;
			break;
		}
		pipeline->apply();
		glLoadMatrixf(glm::value_ptr(view.view));
		if (instance->skin < 0)
			glMultMatrixf(glm::value_ptr(instance->world));
		else {
			auto &palette = pose.palettes[instance->skin];
			glUniformMatrix4fv(pipeline->uniform("joints[0]"), (GLsizei)palette.size(), GL_FALSE,
			                   glm::value_ptr(palette[0]));
		}
		auto uniform = [&](const char *n) { return pipeline->uniform(n); };
		glUniform1f(uniform("mirrorSign"), instance->determinant < 0 ? -1.f : 1.f);
		glUniform4f(uniform("baseFactor"), material.base_color[0], material.base_color[1],
		            material.base_color[2], material.base_color[3]);
		glUniform3f(uniform("emissiveFactor"), material.emissive[0], material.emissive[1],
		            material.emissive[2]);
		glUniform1f(uniform("metallic"), material.metallic);
		glUniform1f(uniform("roughness"), material.roughness);
		glUniform1f(uniform("normalScale"),
		            material.normal_map.texture >= 0 ? material.normal_map.scale : 0);
		glUniform1f(uniform("aoStrength"),
		            material.occlusion_map.texture >= 0 ? material.occlusion_map.scale : 0);
		glUniform1f(uniform("alphaCutoff"), material.alpha_cutoff);
		glUniform1f(uniform("exposure"), view.exposure);
		glUniform1i(uniform("alphaMode"), (int)material.alpha_mode);
		glUniform1i(uniform("unlit"), material.unlit);
		glUniform1i(uniform("debugMode"), view.debug_mode);
		const ice::assets::scene_texture_slot slots[] = {
		    material.base_color_map, material.metal_rough_map, material.normal_map,
		    material.occlusion_map, material.emissive_map};
		const char *samplers[] = {"baseMap", "mrMap", "normalMap", "aoMap", "emissiveMap"};
		const char *uv_names[] = {"baseUV", "mrUV", "normalUV", "aoUV", "emissiveUV"};
		for (int i = 0; i < 5; i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			unsigned texture = slots[i].texture >= 0
			                       ? (unsigned)slots[i].texture
			                       : (unsigned)resources_->textures.size() - (i == 2 ? 1 : 2);
			glBindTexture(GL_TEXTURE_2D, resources_->textures[texture]);
			glUniform1i(uniform(samplers[i]), i);
			glUniform1i(uniform(uv_names[i]), slots[i].uv_set);
		}
		glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
		GLsizei stride = sizeof(ice::assets::scene_vertex);
		for (auto &attribute : pipeline->descriptor().vertices.attributes) {
			const void *offset     = (void *)(std::size_t)attribute.offset;
			GLint       components = (GLint)attribute.format;
			if (attribute.semantic == ice::render::vertex_semantic::position) {
				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(components, GL_FLOAT, stride, offset);
			} else if (attribute.semantic == ice::render::vertex_semantic::normal) {
				glEnableClientState(GL_NORMAL_ARRAY);
				glNormalPointer(GL_FLOAT, stride, offset);
			} else if (attribute.semantic == ice::render::vertex_semantic::color0) {
				glEnableClientState(GL_COLOR_ARRAY);
				glColorPointer(components, GL_FLOAT, stride, offset);
			} else {
				unsigned unit = attribute.semantic == ice::render::vertex_semantic::tex_coord0   ? 0
				                : attribute.semantic == ice::render::vertex_semantic::tex_coord1 ? 1
				                : attribute.semantic == ice::render::vertex_semantic::tangent    ? 2
				                : attribute.semantic == ice::render::vertex_semantic::joints0    ? 3
				                                                                              : 4;
				glClientActiveTexture(GL_TEXTURE0 + unit);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(components, GL_FLOAT, stride, offset);
			}
		}
		glDrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
	}
	render_target::bind_framebuffer(previous_framebuffer);
	glDrawBuffer(previous_framebuffer ? 0x8CE0 : GL_BACK);
	glReadBuffer(previous_framebuffer ? 0x8CE0 : GL_BACK);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, target_.color());
	composite_.bind();
	glUniform1i(composite_.uniform("image"), 0);
	glUniform1i(composite_.uniform("debugMode"), view.debug_mode);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(1, 0);
	glVertex2f(1, -1);
	glTexCoord2f(1, 1);
	glVertex2f(1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(-1, 1);
	glEnd();
	if (view.skeleton) {
		glUseProgram(0);
		for (unsigned unit = 0; unit < 5; unit++) {
			glActiveTexture(GL_TEXTURE0 + unit);
			glDisable(GL_TEXTURE_2D);
		}
		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(glm::value_ptr(view.projection));
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(glm::value_ptr(view.view));
		glColor3f(1, .7f, .15f);
		glLineWidth(2);
		glBegin(GL_LINES);
		for (auto edge : resources_->bones) {
			glVertex3fv(glm::value_ptr(pose.world[edge.first]) + 12);
			glVertex3fv(glm::value_ptr(pose.world[edge.second]) + 12);
		}
		glEnd();
		glPointSize(4);
		glBegin(GL_POINTS);
		for (auto joint : resources_->joint_nodes)
			glVertex3fv(glm::value_ptr(pose.world[joint]) + 12);
		glEnd();
	}
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(matrix_mode);
	glUseProgram(previous_program);
	glPopClientAttrib();
	glPopAttrib();
	glBindBuffer(GL_ARRAY_BUFFER, old_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, old_ibo);
	glClientActiveTexture(client_active);
	glActiveTexture(active);
	if (glGetError() != GL_NO_ERROR) {
		diagnostics = "GL scene draw/state restoration failed";
		return false;
	}
	return ok;
}
} // namespace
} // namespace ice::render::gl
std::unique_ptr<ice::render::scene_renderer>
ice::render::create_scene_renderer_gl(std::shared_ptr<ice::io::resource_resolver> resolver) {
	return std::unique_ptr<ice::render::scene_renderer>(
	    new ice::render::gl::scene_gl(std::move(resolver)));
}
