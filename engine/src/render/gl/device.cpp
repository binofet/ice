#include "ice/render/gl/device.h"
#include "api.h"
#include "ice/core/utf8.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>
namespace ice::render::gl {
namespace detail {
struct device_state {
	std::shared_ptr<native_context> context;
	functions                       api;
	GLuint                          vao = 0, uniforms = 0;
	std::string                     error;
	~device_state() {
		if (context && context->make_current()) {
			if (vao && api.DeleteVertexArrays)
				api.DeleteVertexArrays(1, &vao);
			if (uniforms && api.DeleteBuffers)
				api.DeleteBuffers(1, &uniforms);
		}
	}
};
} // namespace detail
struct pipeline::implementation {
	std::shared_ptr<detail::device_state> state;
	GLuint                                program       = 0;
	std::size_t                           uniform_bytes = 0;
	bool                                  samples_image = false;
	~implementation() {
		if (program && state->context->make_current())
			state->api.DeleteProgram(program);
	}
};
struct texture::implementation {
	std::shared_ptr<detail::device_state> state;
	GLuint                                name  = 0;
	int                                   width = 0, height = 0;
	~implementation() {
		if (name && state->context->make_current())
			state->api.DeleteTextures(1, &name);
	}
};
struct presentation_surface::implementation {
	std::shared_ptr<detail::device_state>   state;
	std::shared_ptr<detail::native_context> context;
	GLuint                                  vao = 0, uniforms = 0;
	int                                     width = 0, height = 0;
	~implementation() {
		if (context && context->make_current()) {
			state->api.Finish();
			if (vao)
				state->api.DeleteVertexArrays(1, &vao);
			if (uniforms)
				state->api.DeleteBuffers(1, &uniforms);
		}
	}
};
namespace {
ice::unexpected<diagnostic> failure(std::string message) {
	return ice::unexpected(diagnostic{std::move(message)});
}
bool identifier(const std::string &text) {
	if (text.empty() || text.size() > 128 ||
	    !(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_'))
		return false;
	return std::all_of(text.begin(), text.end(),
	                   [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}
std::string check(detail::device_state &state) {
	GLenum error = state.api.GetError();
	if (!error)
		return {};
	state.error = "OpenGL command error " + std::to_string(error);
	// Drain ordinary errors without risking an unbounded loop on a lost or broken context.
	for (int remaining = 15; remaining > 0 && state.api.GetError() != GL_NO_ERROR; --remaining) {
	}
	return state.error;
}
std::string shader_log(detail::functions &api, GLuint object, bool program) {
	GLint size = 0;
	if (program)
		api.GetProgramiv(object, GL_INFO_LOG_LENGTH, &size);
	else
		api.GetShaderiv(object, GL_INFO_LOG_LENGTH, &size);
	std::string log(std::size_t(std::clamp(size, 1, 65536)), '\0');
	if (program)
		api.GetProgramInfoLog(object, GLsizei(log.size()), nullptr, log.data());
	else
		api.GetShaderInfoLog(object, GLsizei(log.size()), nullptr, log.data());
	log.resize(std::strlen(log.c_str()));
	return log;
}
ice::expected<GLuint, diagnostic> compile(detail::functions &api, GLenum stage,
                                          const std::string &source, const std::string &entry,
                                          const std::string &uri) {
	std::string prefix = "#version 410 core\n#define ";
	prefix += stage == GL_VERTEX_SHADER ? "ICE_VERTEX_STAGE\n" : "ICE_FRAGMENT_STAGE\n";
	if (entry != "main")
		prefix += "#define " + entry + " main\n";
	prefix += "#line 1\n";
	const GLchar *pieces[] = {prefix.c_str(), source.c_str()};
	GLuint        shader   = api.CreateShader(stage);
	if (!shader)
		return failure(uri + ": shader allocation failed");
	api.ShaderSource(shader, 2, pieces, nullptr);
	api.CompileShader(shader);
	GLint status = 0;
	api.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		auto error = shader_log(api, shader, false);
		api.DeleteShader(shader);
		return failure(uri + ": " + error);
	}
	return shader;
}
bool valid_draw(const std::shared_ptr<detail::device_state> &state,
                const std::shared_ptr<detail::device_state> &pipeline_state,
                std::span<const std::byte> uniforms, std::size_t required) {
	return state == pipeline_state && uniforms.size() >= required && uniforms.size() <= 4096;
}
ice::expected<GLuint, diagnostic> upload_texture(detail::device_state &state, const image &pixels) {
	if (pixels.width < 1 || pixels.height < 1 || pixels.width > 4096 || pixels.height > 4096 ||
	    pixels.rgba.size() != std::size_t(pixels.width) * std::size_t(pixels.height) * 4)
		return failure(
		    "Texture dimensions must be in [1,4096] with exactly width*height*4 RGBA bytes");
	if (!state.context->make_current())
		return failure("Cannot activate OpenGL texture preparation context");
	auto  &api  = state.api;
	GLuint name = 0;
	api.GenTextures(1, &name);
	if (!name)
		return failure("OpenGL texture allocation failed");
	api.ActiveTexture(GL_TEXTURE0);
	api.BindTexture(GL_TEXTURE_2D, name);
	api.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	api.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
	api.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	api.PixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	api.PixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	// Keep top-row-first bytes in their authored order. A shader using top-origin UVs
	// samples the first uploaded row at v=0; OpenGL capture normalization stays separate.
	api.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixels.width, pixels.height, 0, GL_RGBA,
	               GL_UNSIGNED_BYTE, pixels.rgba.data());
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	// Shared presentation contexts must see the completed upload before sampling it.
	api.Finish();
	if (auto error = check(state); !error.empty()) {
		api.DeleteTextures(1, &name);
		return failure(error);
	}
	return name;
}
void encode(detail::functions &api, GLuint program, GLuint vao, GLuint buffer, int width,
            int height, std::span<const std::byte> uniforms, GLuint sampled_image) {
	api.Viewport(0, 0, width, height);
	api.Disable(GL_DEPTH_TEST);
	api.Disable(GL_BLEND);
	api.Disable(GL_CULL_FACE);
	api.Disable(GL_SCISSOR_TEST);
	api.Disable(GL_FRAMEBUFFER_SRGB);
	api.ClearColor(0, 0, 0, 1);
	api.Clear(GL_COLOR_BUFFER_BIT);
	api.UseProgram(program);
	api.BindVertexArray(vao);
	api.ActiveTexture(GL_TEXTURE0);
	api.BindTexture(GL_TEXTURE_2D, sampled_image);
	if (!uniforms.empty()) {
		// Orphaning gives the driver permission to retain bytes referenced by prior draws.
		// Each surface owns its own UBO; independent contexts never race shared uniform bytes.
		api.BindBuffer(GL_UNIFORM_BUFFER, buffer);
		api.BufferData(GL_UNIFORM_BUFFER, GLsizeiptr(uniforms.size()), uniforms.data(),
		               GL_STREAM_DRAW);
		api.BindBufferBase(GL_UNIFORM_BUFFER, 0, buffer);
	}
	api.DrawArrays(GL_TRIANGLES, 0, 3);
}
} // namespace
pipeline::pipeline(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
pipeline::~pipeline() = default;
texture::texture(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
texture::~texture() = default;
ice::expected<void, diagnostic> texture::update(const image &pixels) {
	auto &data = *implementation_;
	if (pixels.width != data.width || pixels.height != data.height)
		return failure("Texture update must preserve dimensions");
	auto uploaded = upload_texture(*data.state, pixels);
	if (!uploaded)
		return ice::unexpected(uploaded.error());
	// Allocate replacement storage rather than writing into an image still sampled by
	// queued work in another context. GL retains old storage while draws/bindings use it.
	auto previous = std::exchange(data.name, *uploaded);
	data.state->api.DeleteTextures(1, &previous);
	return {};
}
presentation_surface::presentation_surface(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
presentation_surface::~presentation_surface() = default;
device::device(std::shared_ptr<detail::device_state> state) : state_(std::move(state)) {}
device::~device() = default;
ice::expected<std::unique_ptr<device>, diagnostic> device::create() {
	auto context = detail::create_context();
	if (!context)
		return failure(context.error());
	auto state     = std::make_shared<detail::device_state>();
	state->context = std::move(*context);
	std::string error;
	if (!state->api.load(*state->context, error))
		return failure(error);
	GLint major = 0, minor = 0, profile = 0;
	state->api.GetIntegerv(GL_MAJOR_VERSION, &major);
	state->api.GetIntegerv(GL_MINOR_VERSION, &minor);
	state->api.GetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
	if (major < 4 || (major == 4 && minor < 1) || !(profile & GL_CONTEXT_CORE_PROFILE_BIT))
		return failure("OpenGL 4.1 or newer core profile is required");
	state->api.GenVertexArrays(1, &state->vao);
	state->api.GenBuffers(1, &state->uniforms);
	if (auto error = check(*state); !error.empty())
		return failure(error);
	// The unique owner moves into tl::expected; clang-analyzer 18 loses that ownership path.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<device>(new device(std::move(state)));
}
std::string device::name() const {
	if (!state_->context->make_current())
		return "OpenGL (context unavailable)";
	auto renderer = reinterpret_cast<const char *>(state_->api.GetString(GL_RENDERER));
	auto version  = reinterpret_cast<const char *>(state_->api.GetString(GL_VERSION));
	return std::string(renderer ? renderer : "OpenGL") + " / " +
	       (version ? version : "unknown version");
}
std::string device::take_error() { return std::exchange(state_->error, {}); }
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const ice::io::resource_resolver &resolver, const std::string &uri,
                         const std::string &vertex_entry, const std::string &fragment_entry) {
	auto snapshot = shader_snapshot::read(resolver, uri);
	if (!snapshot)
		return ice::unexpected(snapshot.error());
	return prepare_pipeline(*snapshot, vertex_entry, fragment_entry);
}
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const shader_snapshot &snapshot, const std::string &vertex_entry,
                         const std::string &fragment_entry) {
	const auto &uri = snapshot.uri();
	std::string source(snapshot.source());
	if (!ice::core::valid_utf8_text(source, 1024 * 1024) || source.empty() ||
	    !identifier(vertex_entry) || !identifier(fragment_entry))
		return failure(uri + ": invalid GLSL source or entry point");
	if (!state_->context->make_current())
		return failure("Cannot activate OpenGL preparation context");
	auto &api    = state_->api;
	auto  vertex = compile(api, GL_VERTEX_SHADER, source, vertex_entry, uri);
	if (!vertex)
		return ice::unexpected(vertex.error());
	auto fragment = compile(api, GL_FRAGMENT_SHADER, source, fragment_entry, uri);
	if (!fragment) {
		api.DeleteShader(*vertex);
		return ice::unexpected(fragment.error());
	}
	auto prepared     = std::make_unique<pipeline::implementation>();
	prepared->state   = state_;
	prepared->program = api.CreateProgram();
	if (!prepared->program) {
		api.DeleteShader(*vertex);
		api.DeleteShader(*fragment);
		return failure(uri + ": program allocation failed");
	}
	api.AttachShader(prepared->program, *vertex);
	api.AttachShader(prepared->program, *fragment);
	api.LinkProgram(prepared->program);
	api.DeleteShader(*vertex);
	api.DeleteShader(*fragment);
	GLint linked = 0;
	api.GetProgramiv(prepared->program, GL_LINK_STATUS, &linked);
	if (!linked)
		return failure(uri + ": " + shader_log(api, prepared->program, true));
	GLint attributes = 0, blocks = 0, uniforms = 0;
	api.GetProgramiv(prepared->program, GL_ACTIVE_ATTRIBUTES, &attributes);
	api.GetProgramiv(prepared->program, GL_ACTIVE_UNIFORM_BLOCKS, &blocks);
	api.GetProgramiv(prepared->program, GL_ACTIVE_UNIFORMS, &uniforms);
	if (blocks > 1)
		return failure(uri + ": initial pipelines support one std140 uniform block");
	// Apple reports built-in gl_VertexID in GL_ACTIVE_ATTRIBUTES. It has no assignable
	// client vertex data. Names beginning gl_ are reserved by GLSL for built-ins;
	// reject only real application vertex inputs.
	GLint attribute_name_bytes = 0;
	api.GetProgramiv(prepared->program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attribute_name_bytes);
	if (attribute_name_bytes < 0 || attribute_name_bytes > 65536)
		return failure(uri + ": invalid attribute reflection name bounds");
	std::vector<char> attribute_name(std::max(attribute_name_bytes, 1));
	for (GLint index = 0; index < attributes; ++index) {
		GLsizei length = 0;
		GLint   size   = 0;
		GLenum  type   = 0;
		api.GetActiveAttrib(prepared->program, GLuint(index), GLsizei(attribute_name.size()),
		                    &length, &size, &type, attribute_name.data());
		if (!std::string_view(attribute_name.data()).starts_with("gl_") &&
		    api.GetAttribLocation(prepared->program, attribute_name.data()) >= 0)
			return failure(
			    uri + ": initial pipelines support procedural vertices without vertex attributes");
	}
	GLint uniform_name_bytes = 0;
	api.GetProgramiv(prepared->program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_name_bytes);
	if (uniform_name_bytes < 0 || uniform_name_bytes > 65536)
		return failure(uri + ": invalid uniform reflection name bounds");
	std::vector<char> uniform_name(std::max(uniform_name_bytes, 1));
	for (GLint index = 0; index < uniforms; ++index) {
		GLuint uniform = GLuint(index);
		GLint  block   = -1;
		api.GetActiveUniformsiv(prepared->program, 1, &uniform, GL_UNIFORM_BLOCK_INDEX, &block);
		if (block >= 0)
			continue;
		GLsizei length = 0;
		GLint   size   = 0;
		GLenum  type   = 0;
		api.GetActiveUniform(prepared->program, uniform, GLsizei(uniform_name.size()), &length,
		                     &size, &type, uniform_name.data());
		if (type != GL_SAMPLER_2D || size != 1 || prepared->samples_image ||
		    std::string_view(uniform_name.data()).find('[') != std::string_view::npos)
			return failure(uri +
			               ": pipelines support one non-array sampler2D and uniform block zero");
		GLint location = api.GetUniformLocation(prepared->program, uniform_name.data());
		GLint binding  = -1;
		if (location >= 0)
			api.GetUniformiv(prepared->program, location, &binding);
		if (binding != 0)
			return failure(uri + ": sampler2D must use texture unit zero");
		api.UseProgram(prepared->program);
		api.Uniform1i(location, 0);
		prepared->samples_image = true;
	}
	if (blocks) {
		GLint size = 0;
		api.GetActiveUniformBlockiv(prepared->program, 0, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
		if (size < 0 || size > 4096)
			return failure(uri + ": uniform block exceeds 4096 bytes");
		prepared->uniform_bytes = std::size_t(size);
		api.UniformBlockBinding(prepared->program, 0, 0);
	}
	// Programs are shared with independent presentation contexts. Complete publication on
	// the preparation context before another context binds the newly linked program.
	api.Finish();
	if (auto error = check(*state_); !error.empty())
		return failure(uri + ": " + error);
	// Ownership transfers into tl::expected rather than escaping as a raw allocation.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<pipeline>(new pipeline(std::move(prepared)));
}
ice::expected<std::unique_ptr<texture>, diagnostic> device::create_texture(const image &pixels) {
	auto data     = std::make_unique<texture::implementation>();
	data->state   = state_;
	auto uploaded = upload_texture(*state_, pixels);
	if (!uploaded)
		return ice::unexpected(uploaded.error());
	data->name   = *uploaded;
	data->width  = pixels.width;
	data->height = pixels.height;
	// Ownership transfers into tl::expected rather than escaping as a raw allocation.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<texture>(new texture(std::move(data)));
}
ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
device::attach(ice::platform::desktop_window &window) {
	auto handle  = window.native_handle();
	auto context = detail::create_context(state_->context, &handle);
	if (!context)
		return failure(context.error());
	auto data     = std::make_unique<presentation_surface::implementation>();
	data->state   = state_;
	data->context = std::move(*context);
	state_->api.GenVertexArrays(1, &data->vao);
	state_->api.GenBuffers(1, &data->uniforms);
	if (auto error = check(*state_); !error.empty())
		return failure(error);
	// Ownership transfers into tl::expected below; the native surface owns both contexts.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	auto result = std::unique_ptr<presentation_surface>(new presentation_surface(std::move(data)));
	auto size   = window.extent();
	result->resize(size.pixel_width, size.pixel_height);
	return result;
}
bool presentation_surface::resize(int width, int height) {
	if (width < 0 || height < 0 || width > 16384 || height > 16384)
		return false;
	auto &data = *implementation_;
	if (data.width == width && data.height == height)
		return true;
	data.width  = width;
	data.height = height;
	data.context->resize(width, height);
	return true;
}
ice::expected<frame_status, diagnostic>
presentation_surface::draw(const pipeline &pipeline, std::span<const std::byte> uniforms,
                           const texture *sampled_image) {
	auto &data     = *implementation_;
	auto &prepared = *pipeline.implementation_;
	if (!valid_draw(data.state, prepared.state, uniforms, prepared.uniform_bytes))
		return failure("Foreign pipeline or invalid uniform byte count");
	if (sampled_image && sampled_image->implementation_->state != data.state)
		return failure("Foreign sampled texture");
	if (prepared.samples_image && !sampled_image)
		return failure("Pipeline requires a sampled texture");
	if (!data.width || !data.height)
		return frame_status::unavailable;
	if (!data.context->make_current())
		return failure("Cannot activate OpenGL presentation context");
	data.state->api.BindFramebuffer(GL_FRAMEBUFFER, 0);
	encode(data.state->api, prepared.program, data.vao, data.uniforms, data.width, data.height,
	       uniforms, sampled_image ? sampled_image->implementation_->name : 0);
	if (auto error = check(*data.state); !error.empty())
		return failure(error);
	if (!data.context->present())
		return failure("Native OpenGL buffer presentation failed");
	return frame_status::submitted;
}
ice::expected<void, diagnostic> presentation_surface::wait_idle() {
	auto &data = *implementation_;
	if (!data.context->make_current())
		return failure("Cannot activate OpenGL presentation context");
	data.state->api.Finish();
	if (auto error = check(*data.state); !error.empty())
		return failure(error);
	return {};
}

ice::expected<image, diagnostic> device::render_offscreen(const pipeline &pipeline, int width,
                                                          int                        height,
                                                          std::span<const std::byte> uniforms,
                                                          const texture *sampled_image) {
	auto &prepared = *pipeline.implementation_;
	if (width < 1 || height < 1 || width > 4096 || height > 4096 ||
	    !valid_draw(state_, prepared.state, uniforms, prepared.uniform_bytes))
		return failure("Invalid capture dimensions, foreign pipeline or uniform byte count");
	if (sampled_image && sampled_image->implementation_->state != state_)
		return failure("Foreign sampled texture");
	if (prepared.samples_image && !sampled_image)
		return failure("Pipeline requires a sampled texture");
	if (!state_->context->make_current())
		return failure("Cannot activate OpenGL capture context");
	auto &api = state_->api;
	struct target_owner {
		detail::functions &api;
		GLuint             texture = 0, framebuffer = 0;
		~target_owner() {
			api.BindFramebuffer(GL_FRAMEBUFFER, 0);
			if (framebuffer)
				api.DeleteFramebuffers(1, &framebuffer);
			if (texture)
				api.DeleteTextures(1, &texture);
		}
	} target{api};
	api.GenTextures(1, &target.texture);
	api.BindTexture(GL_TEXTURE_2D, target.texture);
	api.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
	               nullptr);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	api.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	api.GenFramebuffers(1, &target.framebuffer);
	api.BindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	api.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture,
	                         0);
	if (api.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return failure("OpenGL capture framebuffer is incomplete");
	encode(api, prepared.program, state_->vao, state_->uniforms, width, height, uniforms,
	       sampled_image ? sampled_image->implementation_->name : 0);
	image result{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
	api.PixelStorei(GL_PACK_ALIGNMENT, 1);
	api.ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.rgba.data());
	if (auto error = check(*state_); !error.empty())
		return failure(error);
	// Normalize OpenGL's bottom-origin readback to the common top-row-first capture contract.
	std::size_t stride = std::size_t(width) * 4;
	for (int y = 0; y < height / 2; ++y)
		std::swap_ranges(result.rgba.begin() + std::size_t(y) * stride,
		                 result.rgba.begin() + std::size_t(y + 1) * stride,
		                 result.rgba.begin() + std::size_t(height - y - 1) * stride);
	return result;
}
} // namespace ice::render::gl
