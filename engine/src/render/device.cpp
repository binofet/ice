#include "ice/render/device.h"
#include "batch_validation.h"
#include <atomic>
#include <limits>
#include <set>
#ifdef ICE_RENDER_METAL
#include "ice/render/metal/device.h"
#endif
#ifdef ICE_RENDER_OPENGL
#include "ice/render/gl/device.h"
#endif
namespace ice::render {
namespace {
struct window_lease {
	ice::platform::window_id                   id;
	static std::set<ice::platform::window_id> &active() {
		// Process lifetime, like the platform event registry: a global presentation owner
		// may retire after other function statics during executable shutdown.
		static auto *windows = new std::set<ice::platform::window_id>;
		return *windows;
	}
	explicit window_lease(ice::platform::window_id id) : id(id) { active().insert(id); }
	~window_lease() { active().erase(id); }
};
ice::unexpected<diagnostic> failure(const char *message) {
	return ice::unexpected(diagnostic{message});
}
} // namespace
// The token identifies a logical device, not its physical GPU. It survives its C++ device owner
// through prepared resources, preventing a same-GPU foreign pipeline from slipping into a draw.
struct device::implementation {
	backend_kind               kind;
	std::uint64_t              identity = 0;
	std::shared_ptr<const int> token    = std::make_shared<const int>(0);
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::device> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::device> opengl;
#endif
};
struct pipeline::implementation {
	std::shared_ptr<const int> token;
	pipeline_options           options;
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::pipeline> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::pipeline> opengl;
#endif
};
struct mesh::implementation {
	std::shared_ptr<const int> token;
	vertex_layout              vertices = vertex_layout::position_normal;
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::mesh> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::mesh> opengl;
#endif
};
struct texture::implementation {
	std::shared_ptr<const int> token;
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::texture> metal;
	const metal::texture           *metal_view = nullptr;
	const metal::texture *native_metal() const { return metal ? metal.get() : metal_view; }
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::texture> opengl;
	const gl::texture           *gl_view = nullptr;
	const gl::texture           *native_gl() const { return opengl ? opengl.get() : gl_view; }
#endif
};
struct render_target::implementation {
	std::shared_ptr<const int> token;
	render_target_description  description;
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::render_target> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::render_target> opengl;
#endif
	// Destroy borrowed wrappers before the native target which owns their textures.
	std::unique_ptr<texture> color, depth;
};
struct borrowed_destination::implementation {
	std::shared_ptr<const int> token;
	render_target_description  description;
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::borrowed_destination> opengl;
#endif
};
struct presentation_surface::implementation {
	// Destroy the native endpoint before releasing its exclusive logical-window lease.
	std::unique_ptr<window_lease> lease;
	std::shared_ptr<const int>    token;
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::presentation_surface> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::presentation_surface> opengl;
#endif
};
namespace detail {
struct batch_access {
	static ice::expected<void, diagnostic>
	validate(const std::shared_ptr<const int> &token, const frame_batch &batch,
	         color_format format = color_format::rgba8_unorm) {
		auto shape = validate_batch_shape(batch);
		if (!shape)
			return shape;
		for (const auto &item : batch.draws) {
			const auto &prepared = *item.pipeline->implementation_;
			if (prepared.token != token ||
			    (item.geometry && item.geometry->implementation_->token != token))
				return failure("Frame contains a foreign pipeline or mesh");
			if (prepared.options.target_format != format)
				return failure("Pipeline color format does not match the destination");
			for (unsigned slot = 0; slot < 4; ++slot) {
				auto sampled = sampled_at(item, slot);
				if (sampled && sampled->implementation_->token != token)
					return failure("Frame contains a foreign sampled image");
			}
			if (prepared.options.vertices != (item.geometry
			                                      ? item.geometry->implementation_->vertices
			                                      : vertex_layout::procedural))
				return failure("Pipeline and mesh vertex layouts do not match");
		}
		return {};
	}
#ifdef ICE_RENDER_METAL
	static std::vector<metal::draw_item> metal_items(const frame_batch &batch) {
		std::vector<metal::draw_item> result;
		result.reserve(batch.draws.size());
		for (const auto &item : batch.draws)
			result.push_back(
			    {item.pipeline->implementation_->metal.get(),
			     item.geometry ? item.geometry->implementation_->metal.get() : nullptr,
			     item.uniforms,
			     item.sampled_image ? item.sampled_image->implementation_->native_metal() : nullptr,
			     {}});
		for (std::size_t index = 0; index < batch.draws.size(); ++index)
			for (unsigned slot = 0; slot < 4; ++slot)
				if (auto sampled = batch.draws[index].sampled_images[slot])
					result[index].sampled_images[slot] = sampled->implementation_->native_metal();
		return result;
	}
#endif
#ifdef ICE_RENDER_OPENGL
	static std::vector<gl::draw_item> gl_items(const frame_batch &batch) {
		std::vector<gl::draw_item> result;
		result.reserve(batch.draws.size());
		for (const auto &item : batch.draws)
			result.push_back(
			    {item.pipeline->implementation_->opengl.get(),
			     item.geometry ? item.geometry->implementation_->opengl.get() : nullptr,
			     item.uniforms,
			     item.sampled_image ? item.sampled_image->implementation_->native_gl() : nullptr,
			     {}});
		for (std::size_t index = 0; index < batch.draws.size(); ++index)
			for (unsigned slot = 0; slot < 4; ++slot)
				if (auto sampled = batch.draws[index].sampled_images[slot])
					result[index].sampled_images[slot] = sampled->implementation_->native_gl();
		return result;
	}
#endif
};
} // namespace detail
device::device(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
device::~device() = default;
pipeline::pipeline(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
pipeline::~pipeline() = default;
mesh::mesh(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
mesh::~mesh() = default;
texture::texture(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
texture::~texture() = default;
render_target::render_target(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
render_target::~render_target() = default;
const texture            &render_target::color() const { return *implementation_->color; }
const texture            &render_target::depth() const { return *implementation_->depth; }
render_target_description render_target::description() const {
	return implementation_->description;
}
ice::expected<void, diagnostic> texture::update(const image &pixels) {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->update(pixels);
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->update(pixels);
#endif
	return failure("Only owned RGBA8 upload textures support CPU updates");
}
presentation_surface::presentation_surface(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
presentation_surface::~presentation_surface() = default;
backend_kind  device::backend() const { return implementation_->kind; }
std::uint64_t device::identity() const { return implementation_->identity; }
ice::expected<std::unique_ptr<device>, diagnostic> device::create(backend_kind backend) {
	auto result  = std::make_unique<implementation>();
	result->kind = backend;
	static std::atomic<std::uint64_t> next_identity{1};
	auto                              identity = next_identity.load(std::memory_order_relaxed);
	do {
		if (identity == std::numeric_limits<std::uint64_t>::max())
			return failure("Logical graphics device identities exhausted");
	} while (
	    !next_identity.compare_exchange_weak(identity, identity + 1, std::memory_order_relaxed));
	result->identity = identity;
#ifdef ICE_RENDER_METAL
	if (backend == backend_kind::metal) {
		auto native = metal::device::create();
		if (!native)
			return ice::unexpected(native.error());
		result->metal = std::move(*native);
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<device>(new device(std::move(result)));
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (backend == backend_kind::opengl || backend == backend_kind::opengles) {
		auto native = gl::device::create(
		    backend == backend_kind::opengles ? gl::profile::embedded : gl::profile::desktop_core);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<device>(new device(std::move(result)));
	}
#endif
	return failure("Requested graphics backend is not available in this host/build");
}
std::string device::name() const {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->name();
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->name();
#endif
	return {};
}
std::string device::take_error() {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->take_error();
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->take_error();
#endif
	return {};
}
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const ice::io::resource_resolver &resolver, const shader_source &source,
                         pipeline_options options) {
	auto snapshot = shader_snapshot::read(resolver, source.uri);
	if (!snapshot)
		return ice::unexpected(snapshot.error());
	return prepare_pipeline(*snapshot, source.vertex_entry, source.fragment_entry, options);
}
ice::expected<std::unique_ptr<pipeline>, diagnostic>
device::prepare_pipeline(const shader_snapshot &source, const std::string &vertex_entry,
                         const std::string &fragment_entry, pipeline_options options) {
	if (!detail::valid_options(options))
		return failure("Invalid vertex layout or depth-write without depth-test");
	auto result     = std::make_unique<pipeline::implementation>();
	result->token   = implementation_->token;
	result->options = options;
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto native =
		    implementation_->metal->prepare_pipeline(source, vertex_entry, fragment_entry, options);
		if (!native)
			return ice::unexpected(native.error());
		result->metal = std::move(*native);
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->prepare_pipeline(source, vertex_entry,
		                                                        fragment_entry, options);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
	}
#endif
	// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<pipeline>(new pipeline(std::move(result)));
}
ice::expected<std::unique_ptr<render_target>, diagnostic>
device::create_render_target(const render_target_description &description) {
	if (!detail::valid_target(description))
		return failure("Invalid render target dimensions or color format");
	auto result         = std::make_unique<render_target::implementation>();
	result->token       = implementation_->token;
	result->description = description;
	auto color          = std::make_unique<texture::implementation>();
	auto depth          = std::make_unique<texture::implementation>();
	color->token = depth->token = implementation_->token;
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto native = implementation_->metal->create_render_target(description);
		if (!native)
			return ice::unexpected(native.error());
		result->metal     = std::move(*native);
		color->metal_view = &result->metal->color();
		depth->metal_view = &result->metal->depth();
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->create_render_target(description);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
		color->gl_view = &result->opengl->color();
		depth->gl_view = &result->opengl->depth();
	}
#endif
	result->color.reset(new texture(std::move(color)));
	result->depth.reset(new texture(std::move(depth)));
	// Ownership transfers through tl::expected.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<render_target>(new render_target(std::move(result)));
}
ice::expected<frame_status, diagnostic> device::render_into(render_target     &target,
                                                            const frame_batch &batch) {
	auto &output = *target.implementation_;
	if (output.token != implementation_->token)
		return failure("Render target belongs to a different logical device");
	auto valid =
	    detail::batch_access::validate(implementation_->token, batch, output.description.format);
	if (!valid)
		return ice::unexpected(valid.error());
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto items = detail::batch_access::metal_items(batch);
		return implementation_->metal->render_into(*output.metal, {items, batch.clear_color});
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto items = detail::batch_access::gl_items(batch);
		return implementation_->opengl->render_into(*output.opengl, {items, batch.clear_color});
	}
#endif
	return failure("Graphics backend unavailable");
}
borrowed_destination::borrowed_destination(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
borrowed_destination::~borrowed_destination() = default;
render_target_description borrowed_destination::description() const {
	return implementation_->description;
}
ice::expected<acquisition_lease, diagnostic>
borrowed_destination::begin_acquisition(std::uint64_t generation) {
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->begin_acquisition(generation);
#else
	(void)generation;
#endif
	return failure("Borrowed image acquisition is unsupported by this backend");
}
ice::expected<gl::egl_binding, diagnostic> device::egl_context() const {
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->egl_context();
#endif
	return failure("Owned EGL context is unsupported by this backend/build");
}
ice::expected<std::unique_ptr<borrowed_destination>, diagnostic>
device::import_destination(const gl::destination_description &description) {
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->import_destination(description);
		if (!native)
			return ice::unexpected(native.error());
		auto result         = std::make_unique<borrowed_destination::implementation>();
		result->token       = implementation_->token;
		result->description = (*native)->description();
		result->opengl      = std::move(*native);
		// Ownership transfers through tl::expected.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<borrowed_destination>(new borrowed_destination(std::move(result)));
	}
#else
	(void)description;
#endif
	return failure("Borrowed GL destinations are unsupported by this backend/build");
}
ice::expected<frame_status, diagnostic> device::render_into(borrowed_destination    &destination,
                                                            const acquisition_lease &lease,
                                                            const frame_batch       &batch) {
	if (destination.implementation_->token != implementation_->token)
		return failure("Borrowed destination belongs to a different logical device");
	auto valid = detail::batch_access::validate(implementation_->token, batch,
	                                            destination.description().format);
	if (!valid)
		return ice::unexpected(valid.error());
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl && destination.implementation_->opengl) {
		auto items = detail::batch_access::gl_items(batch);
		return implementation_->opengl->render_into(*destination.implementation_->opengl, lease,
		                                            {items, batch.clear_color});
	}
#else
	(void)lease;
#endif
	return failure("Borrowed destination rendering is unsupported by this backend/build");
}
ice::expected<void, diagnostic> device::flush() {
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->flush();
#endif
	return failure("Explicit GL flush is unsupported by this backend/build");
}
ice::expected<void, diagnostic> device::wait_idle() {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->wait_idle();
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->wait_idle();
#endif
	return failure("Graphics backend unavailable");
}
ice::expected<std::unique_ptr<mesh>, diagnostic> device::create_mesh(const mesh_data &description) {
	return upload_mesh(description, vertex_layout::position_normal);
}
ice::expected<std::unique_ptr<mesh>, diagnostic>
device::create_mesh(const textured_mesh_data &description) {
	return upload_mesh(description, vertex_layout::position_normal_uv);
}
ice::expected<std::unique_ptr<mesh>, diagnostic>
device::create_mesh(const skinned_mesh_data &description) {
	return upload_mesh(description, vertex_layout::position_normal_uv_skin);
}
template <typename mesh_description>
ice::expected<std::unique_ptr<mesh>, diagnostic>
device::upload_mesh(const mesh_description &description, vertex_layout layout) {
	auto valid = detail::validate_mesh_data(description);
	if (!valid)
		return ice::unexpected(valid.error());
	auto result      = std::make_unique<mesh::implementation>();
	result->token    = implementation_->token;
	result->vertices = layout;
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto native = implementation_->metal->create_mesh(description);
		if (!native)
			return ice::unexpected(native.error());
		result->metal = std::move(*native);
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->create_mesh(description);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
	}
#endif
	// Ownership transfers into tl::expected rather than escaping as a raw allocation.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<mesh>(new mesh(std::move(result)));
}
ice::expected<std::unique_ptr<texture>, diagnostic> device::create_texture(const image &pixels) {
	auto result   = std::make_unique<texture::implementation>();
	result->token = implementation_->token;
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto native = implementation_->metal->create_texture(pixels);
		if (!native)
			return ice::unexpected(native.error());
		result->metal = std::move(*native);
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->create_texture(pixels);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
	}
#endif
	// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<texture>(new texture(std::move(result)));
}
ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
device::attach(ice::platform::desktop_window &window) {
	if (window_lease::active().contains(window.id()))
		return failure(
		    "A renderer already owns this window; drain and detach it before replacement");
	auto result   = std::make_unique<presentation_surface::implementation>();
	result->token = implementation_->token;
	result->lease = std::make_unique<window_lease>(window.id());
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto native = implementation_->metal->attach(window);
		if (!native)
			return ice::unexpected(native.error());
		result->metal = std::move(*native);
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->attach(window);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
	}
#endif
	// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<presentation_surface>(new presentation_surface(std::move(result)));
}
ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
device::attach(const ice::platform::surface &target) {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal && target.api == ice::platform::surface_api::metal) {
		auto native =
		    implementation_->metal->attach_layer(target.native_handle, target.width, target.height);
		if (!native)
			return ice::unexpected(native.error());
		auto result   = std::make_unique<presentation_surface::implementation>();
		result->token = implementation_->token;
		result->metal = std::move(*native);
		// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
		// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
		return std::unique_ptr<presentation_surface>(new presentation_surface(std::move(result)));
	}
#else
	(void)target;
#endif
	return failure("Native surface API is not supported by the selected backend");
}
ice::expected<image, diagnostic> device::render_offscreen(const pipeline &pipeline, int width,
                                                          int                        height,
                                                          std::span<const std::byte> uniforms,
                                                          const texture *sampled_image) {
	const draw_item item{&pipeline, nullptr, uniforms, sampled_image};
	return render_offscreen(frame_batch{std::span(&item, 1)}, width, height);
}
ice::expected<image, diagnostic> device::render_offscreen(const frame_batch &batch, int width,
                                                          int height) {
	auto valid = detail::batch_access::validate(implementation_->token, batch);
	if (!valid)
		return ice::unexpected(valid.error());
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto items = detail::batch_access::metal_items(batch);
		return implementation_->metal->render_offscreen({items, batch.clear_color}, width, height);
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto items = detail::batch_access::gl_items(batch);
		return implementation_->opengl->render_offscreen({items, batch.clear_color}, width, height);
	}
#endif
	return failure("Graphics backend unavailable");
}
bool presentation_surface::resize(int width, int height) {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->resize(width, height);
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->resize(width, height);
#endif
	return false;
}
ice::expected<frame_status, diagnostic>
presentation_surface::draw(const pipeline &pipeline, std::span<const std::byte> uniforms,
                           const texture *sampled_image) {
	const draw_item item{&pipeline, nullptr, uniforms, sampled_image};
	return draw_batch(frame_batch{std::span(&item, 1)});
}
ice::expected<frame_status, diagnostic> presentation_surface::draw_batch(const frame_batch &batch) {
	auto valid = detail::batch_access::validate(implementation_->token, batch);
	if (!valid)
		return ice::unexpected(valid.error());
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto items = detail::batch_access::metal_items(batch);
		return implementation_->metal->draw_batch({items, batch.clear_color});
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto items = detail::batch_access::gl_items(batch);
		return implementation_->opengl->draw_batch({items, batch.clear_color});
	}
#endif
	return failure("Graphics backend unavailable");
}
ice::expected<void, diagnostic> presentation_surface::wait_idle() {
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->wait_idle();
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->wait_idle();
#endif
	return failure("Graphics backend unavailable");
}
} // namespace ice::render
