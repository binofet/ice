#include "ice/render/device.h"
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
	std::shared_ptr<const int> token = std::make_shared<const int>(0);
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::device> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::device> opengl;
#endif
};
struct pipeline::implementation {
	std::shared_ptr<const int> token;
#ifdef ICE_RENDER_METAL
	std::unique_ptr<metal::pipeline> metal;
#endif
#ifdef ICE_RENDER_OPENGL
	std::unique_ptr<gl::pipeline> opengl;
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
device::device(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
device::~device() = default;
pipeline::pipeline(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
pipeline::~pipeline() = default;
presentation_surface::presentation_surface(std::unique_ptr<implementation> implementation)
    : implementation_(std::move(implementation)) {}
presentation_surface::~presentation_surface() = default;
backend_kind device::backend() const { return implementation_->kind; }
ice::expected<std::unique_ptr<device>, diagnostic> device::create(backend_kind backend) {
	auto result  = std::make_unique<implementation>();
	result->kind = backend;
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
	if (backend == backend_kind::opengl) {
		auto native = gl::device::create();
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
device::prepare_pipeline(const ice::io::resource_resolver &resolver, const shader_source &source) {
	auto result   = std::make_unique<pipeline::implementation>();
	result->token = implementation_->token;
#ifdef ICE_RENDER_METAL
	if (implementation_->metal) {
		auto native = implementation_->metal->prepare_pipeline(
		    resolver, source.uri, source.vertex_entry, source.fragment_entry);
		if (!native)
			return ice::unexpected(native.error());
		result->metal = std::move(*native);
	}
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl) {
		auto native = implementation_->opengl->prepare_pipeline(
		    resolver, source.uri, source.vertex_entry, source.fragment_entry);
		if (!native)
			return ice::unexpected(native.error());
		result->opengl = std::move(*native);
	}
#endif
	// The unique owner moves into tl::expected; analyzer 18 loses that ownership path.
	// NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
	return std::unique_ptr<pipeline>(new pipeline(std::move(result)));
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
                                                          std::span<const std::byte> uniforms) {
	if (implementation_->token != pipeline.implementation_->token)
		return failure("Foreign-device pipeline");
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->render_offscreen(*pipeline.implementation_->metal, width,
		                                                height, uniforms);
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->render_offscreen(*pipeline.implementation_->opengl, width,
		                                                 height, uniforms);
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
presentation_surface::draw(const pipeline &pipeline, std::span<const std::byte> uniforms) {
	if (implementation_->token != pipeline.implementation_->token)
		return failure("Foreign-device pipeline");
#ifdef ICE_RENDER_METAL
	if (implementation_->metal)
		return implementation_->metal->draw(*pipeline.implementation_->metal, uniforms);
#endif
#ifdef ICE_RENDER_OPENGL
	if (implementation_->opengl)
		return implementation_->opengl->draw(*pipeline.implementation_->opengl, uniforms);
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
