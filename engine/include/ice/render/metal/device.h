#pragma once
#include "ice/core/expected.h"
#include "ice/io/resource_uri.h"
#include "ice/platform/desktop_window.h"
#include "ice/render/types.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ice::render::metal {
namespace detail {
/// Private shared native device lifetime; no native object layout crosses this API.
struct device_state;
} // namespace detail
/// Prepared BGRA8, triangle-list render pipeline. Owns native resources independently of a surface.
class pipeline {
public:
	/// Release the pipeline; submitted command buffers retain resources until completion.
	~pipeline();

private:
	struct implementation;
	explicit pipeline(std::unique_ptr<implementation> implementation);
	pipeline(const pipeline &other)                                  = delete;
	pipeline                       &operator=(const pipeline &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
	friend class presentation_surface;
};
/// Native presentation owner. UI/render-thread-only; destroy before the owning view/layer.
/// This first backend supports procedural triangles and optional buffer(0) uniform bytes.
class presentation_surface {
public:
	/// Detach owned presentation state; in-flight commands retain their resources.
	~presentation_surface();
	/// Set drawable pixel dimensions in [0,16384]; zero suspends acquisition.
	bool resize(int width, int height);
	/// Draw three procedural vertices. Uniform bytes are copied, at most 4096, to buffer(0)
	/// in both shader stages. Return busy rather than queue more than three in-flight frames.
	ice::expected<frame_status, diagnostic> draw(const pipeline            &pipeline,
	                                             std::span<const std::byte> uniforms = {});
	/// Finish preceding queue submissions before handing the native view to another renderer.
	ice::expected<void, diagnostic> wait_idle();

private:
	struct implementation;
	explicit presentation_surface(std::unique_ptr<implementation> implementation);
	presentation_surface(const presentation_surface &other)                      = delete;
	presentation_surface           &operator=(const presentation_surface &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
};
/// Explicit Metal device/queue. Creation and preparation run on the owning render thread;
/// compiler/native failures return diagnostics. This is a backend-specific first slice,
/// not a complete portable graphics abstraction or runtime renderer replacement service.
class device {
public:
	/// Create the system's default Metal device and a command queue, or explain failure.
	static ice::expected<std::unique_ptr<device>, diagnostic> create();
	/// Release this owner; surfaces/pipelines retain their shared native device lifetime.
	~device();
	/// Return the selected GPU's native display name.
	std::string name() const;
	/// Read a Metal source library through the URI resolver and prepare the two entry points.
	/// Does not mutate any prior pipeline. Compilation is synchronous; source is limited to 1 MiB
	/// after the provider read. Providers remain responsible for their own read/allocation budgets.
	ice::expected<std::unique_ptr<pipeline>, diagnostic>
	prepare_pipeline(const ice::io::resource_resolver &resolver, const std::string &uri,
	                 const std::string &vertex_entry, const std::string &fragment_entry);
	/// Attach a CAMetalLayer to an AppKit window's borrowed view. Restore its previous layer
	/// on destruction. The caller keeps the window alive; only available on macOS.
	ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
	attach(ice::platform::desktop_window &window);
	/// Use an existing borrowed CAMetalLayer from iOS/iPadOS or another native host.
	/// Reject another live ICE attachment; external renderer ownership is the caller's contract.
	/// The owner outlives the surface and leaves its device/drawable properties to this renderer.
	ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
	attach_layer(void *layer, int width, int height);
	/// Synchronously render/read back a procedural triangle into an owned RGBA8 image.
	/// Dimensions are in [1,4096]. Intended for tests/captures, not the frame loop.
	ice::expected<image, diagnostic> render_offscreen(const pipeline &pipeline, int width,
	                                                  int                        height,
	                                                  std::span<const std::byte> uniforms = {});
	/// Consume a completed asynchronous GPU failure (at most 4096 UTF-8 bytes), or an empty
	/// string; render-thread-only.
	std::string take_error();

private:
	explicit device(std::shared_ptr<detail::device_state> state);
	device(const device &other)                                          = delete;
	device                               &operator=(const device &other) = delete;
	std::shared_ptr<detail::device_state> state_;
};
} // namespace ice::render::metal
