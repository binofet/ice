#pragma once
#include "ice/core/expected.h"
#include "ice/io/resource_uri.h"
#include "ice/platform/desktop_window.h"
#include "ice/render/readback.h"
#include "ice/render/shader_snapshot.h"
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
/// Private resource validation/encoding access; no native layouts cross the API.
struct batch_access;
} // namespace detail
/// Prepared UNORM/HDR triangle-list render pipeline. Owns native resources independently of a
/// surface.
class pipeline {
public:
	/// Release the pipeline; submitted command buffers retain resources until completion.
	~pipeline();

private:
	struct implementation;
	explicit pipeline(std::unique_ptr<implementation> implementation);
	friend struct detail::batch_access;
	pipeline(const pipeline &other)                                  = delete;
	pipeline                       &operator=(const pipeline &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
	friend class presentation_surface;
};
/// Immutable owned vertex/index buffers, with shared native device lifetime.
class mesh {
public:
	/// Release immutable vertex/index storage, retaining any resources already submitted.
	~mesh();

private:
	struct implementation;
	explicit mesh(std::unique_ptr<implementation> implementation);
	friend struct detail::batch_access;
	mesh(const mesh &other)                                      = delete;
	mesh                           &operator=(const mesh &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
	friend class presentation_surface;
};
/// Owned sampled upload or target color/depth view with independent native device lifetime.
class texture {
public:
	/// Release this owner; preceding native submissions retain the sampled storage.
	~texture();
	/// Replace pixels with the same dimensions, preserving the old image on failure.
	ice::expected<void, diagnostic> update(const image &pixels);

private:
	struct implementation;
	explicit texture(std::unique_ptr<implementation> implementation);
	friend struct detail::batch_access;
	texture(const texture &other)                                   = delete;
	texture                        &operator=(const texture &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
	friend class presentation_surface;
	friend class render_target;
};
/// Device-owned, immutable-size color/depth target. Both borrowed texture views remain valid only
/// while this owner lives, and can be sampled after its first successful pass submission.
/// Do not retain a view beyond graph replacement. Submitted GPU commands
/// retain native storage independently. Color is linear/bilinear-clamped; depth is
/// raw/nearest-clamped.
class render_target {
public:
	/// Release both attachments; earlier submissions retain referenced storage.
	~render_target();
	/// Borrow the top-origin color attachment for a later pass; never sample it while rendering
	/// here.
	const texture &color() const;
	/// Borrow top-origin raw zero-to-one depth. Bind only in an explicitly declared depth slot.
	const texture &depth() const;
	/// Return the immutable size and color format used to allocate both attachments.
	render_target_description description() const;

private:
	struct implementation;
	explicit render_target(std::unique_ptr<implementation> implementation);
	render_target(const render_target &other)                             = delete;
	render_target                  &operator=(const render_target &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
	friend struct detail::batch_access;
};
/// One borrowed draw, consumed synchronously. Indexed pipelines require geometry; procedural
/// pipelines require null geometry. Each uniform block is copied before this call returns.
struct draw_item {
	/// Prepared pipeline from the receiving logical device; never null.
	const class pipeline *pipeline = nullptr;
	/// Immutable triangle list, or null for the existing three-vertex procedural draw.
	const mesh *geometry = nullptr;
	/// At most 16384 bytes, satisfying the pipeline's reflected uniform contract.
	std::span<const std::byte> uniforms;
	/// Optional image from the same device; required if the pipeline samples texture zero.
	const texture *sampled_image = nullptr;
	/// Sampled slots zero through three. Legacy sampled_image supplies slot zero only when absent;
	/// two different slot-zero pointers are rejected. All owners live throughout this call.
	std::array<const texture *, 4> sampled_images{};
};
/// Ordered one-pass batch, clearing color/depth exactly once before any draw.
/// Up to 131072 draws and 64 MiB of aggregate uniform bytes. Invalid items reject the whole batch
/// before modifying a target. Empty batches only clear. Blend/depth state belongs to each pipeline.
struct frame_batch {
	/// Borrowed draw/resource descriptions, valid throughout synchronous encoding/submission.
	std::span<const draw_item> draws;
	/// Finite normalized RGBA clear value; each component is in [0,1].
	std::array<float, 4> clear_color{0, 0, 0, 1};
};
/// Native presentation owner; ordered indexed and procedural draws share one color/depth pass.
class presentation_surface {
public:
	/// Detach owned presentation state; in-flight commands retain their resources.
	~presentation_surface();
	/// Validate the entire batch, then submit it with one color/depth clear and ordered draws.
	ice::expected<frame_status, diagnostic> draw_batch(const frame_batch &batch);
	/// Set drawable pixel dimensions in [0,16384]; zero suspends acquisition.
	bool resize(int width, int height);
	/// Draw three procedural vertices. Uniform bytes are copied, at most 16384, to buffer(0)
	/// in both shader stages. Return busy rather than queue more than three in-flight frames.
	ice::expected<frame_status, diagnostic> draw(const pipeline            &pipeline,
	                                             std::span<const std::byte> uniforms = {},
	                                             const texture *sampled_image        = nullptr);
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
	/// Allocate linear RGBA8/RGBA16F color and sampleable Depth32Float storage, without CPU pixels.
	/// Dimensions are in [1,16384] within native limits; failure preserves existing resources.
	ice::expected<std::unique_ptr<render_target>, diagnostic>
	create_render_target(const render_target_description &description);
	/// Validate the entire pass, then encode GPU-only writes ordered before later same-device
	/// passes. No CPU readback/wait. Checked ownership/format/binding/feedback failures preserve
	/// the target. Busy means nothing was submitted; native driver errors do not promise rollback.
	ice::expected<frame_status, diagnostic> render_into(render_target     &target,
	                                                    const frame_batch &batch);
	/// Synchronously drain preceding logical-device work on the render/UI thread. Intended for
	/// detached preparation, tests, handoff or shutdown, never the live postprocess pass chain.
	/// Native failures return diagnostics; driver hangs are not contained by this call.
	ice::expected<void, diagnostic> wait_idle();
	/// Upload a validated immutable position/normal triangle list without retaining CPU aliases.
	ice::expected<std::unique_ptr<mesh>, diagnostic> create_mesh(const mesh_data &description);
	/// Upload finite position/normal/UV vertices, declaring the immutable 32-byte indexed layout.
	ice::expected<std::unique_ptr<mesh>, diagnostic>
	create_mesh(const textured_mesh_data &description);
	/// Upload validated immutable 64-byte position/normal/UV/joints/weights vertices.
	ice::expected<std::unique_ptr<mesh>, diagnostic>
	create_mesh(const skinned_mesh_data &description);
	/// Encode a separate RGBA8 offscreen frame, then read it asynchronously. Dimensions1..4096.
	/// Limits bound all admitted transfers and extracted pixel payloads for this device; byte
	/// charges conservatively include color/depth/staging. Busy submits nothing. All calls stay
	/// on the render thread. No GPU wait or silent synchronous fallback on missing capabilities.
	ice::expected<readback_submission, diagnostic>
	render_offscreen_async(const frame_batch &batch, int width, int height, readback_limits limits);
	/// Synchronously capture an ordered indexed/procedural batch as top-row-first RGBA8.
	/// Dimensions are in [1,4096]. The whole batch is validated before touching its target.
	ice::expected<image, diagnostic> render_offscreen(const frame_batch &batch, int width,
	                                                  int height);
	/// Return the selected GPU's native display name.
	std::string name() const;
	/// Read a Metal source library through the URI resolver and prepare the two entry points.
	/// Does not mutate any prior pipeline. Compilation is synchronous; source is limited to 1 MiB
	/// after the provider read. Providers remain responsible for their own read/allocation budgets.
	ice::expected<std::unique_ptr<pipeline>, diagnostic>
	prepare_pipeline(const ice::io::resource_resolver &resolver, const std::string &uri,
	                 const std::string &vertex_entry, const std::string &fragment_entry,
	                 pipeline_options options = {});
	/// Compile the exact immutable snapshot bytes; no provider is consulted again.
	ice::expected<std::unique_ptr<pipeline>, diagnostic>
	prepare_pipeline(const shader_snapshot &snapshot, const std::string &vertex_entry,
	                 const std::string &fragment_entry, pipeline_options options = {});
	/// Upload one tightly packed RGBA8 image with dimensions in [1,4096].
	/// Sampling is linear/clamped without mipmaps; bind as texture(0)/sampler2D unit zero in either
	/// shader stage.
	ice::expected<std::unique_ptr<texture>, diagnostic> create_texture(const image &pixels);
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
	                                                  std::span<const std::byte> uniforms = {},
	                                                  const texture *sampled_image = nullptr);
	/// Consume a completed asynchronous GPU failure (at most 4096 UTF-8 bytes), or an empty
	/// string; render-thread-only.
	std::string take_error();

private:
	ice::expected<std::unique_ptr<mesh>, diagnostic>
	upload_mesh(std::span<const std::byte> vertices, std::span<const std::uint32_t> indices,
	            vertex_layout layout);
	explicit device(std::shared_ptr<detail::device_state> state);
	device(const device &other)                                          = delete;
	device                               &operator=(const device &other) = delete;
	std::shared_ptr<detail::device_state> state_;
};
} // namespace ice::render::metal
