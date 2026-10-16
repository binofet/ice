#pragma once
#include "ice/core/expected.h"
#include "ice/io/resource_uri.h"
#include "ice/platform/desktop_window.h"
#include "ice/platform/host.h"
#include "ice/render/acquisition_lease.h"
#include "ice/render/gl/egl.h"
#include "ice/render/shader_snapshot.h"
#include "ice/render/types.h"
#include <memory>
#include <span>
namespace ice::render {
namespace detail {
/// Private batch resource access, with no native layout exposed to clients.
struct batch_access;
} // namespace detail
/// Explicit backend selection. Availability depends on the host/build; failures are diagnostics.
enum class backend_kind { metal, opengl, opengles };
/// A backend-specific URI library and its explicit vertex/fragment entry names.
/// Authored shader packages may retain one variant per backend; no implicit source translation
/// occurs.
struct shader_source {
	/// Library identity resolved through configured handlers/mounts.
	std::string uri;
	/// Vertex entry; pipeline_options selects procedural or a declared fixed indexed layout.
	std::string vertex_entry;
	/// Fragment entry; optional buffer(0)/std140 bytes and declared sampled slots are reflected.
	std::string fragment_entry;
};
/// Device-owned prepared pipeline. Keep the authored URI/parameters separately for recreation.
class pipeline {
public:
	/// Release this owner; native backends retain submitted resources according to their API.
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
/// Immutable device-owned indexed triangle list, retaining its declared position/normal[/UV[/skin]]
/// layout. Keep the CPU description separately for recreation on another logical device.
class mesh {
public:
	/// Release vertex/index storage; submitted native commands retain referenced resources.
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
/// Owned RGBA8 upload or borrowed render-target color/depth view. Row zero samples at UV y=0.
/// Updates replace native storage so preceding submissions keep their original image.
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
/// Immutable native color-image wrapper owning only its local framebuffer/depth resources.
/// The runtime owns color storage and must retain it until this wrapper is destroyed. Retire
/// wrappers before native swapchain/session/context teardown; destruction never releases an image.
/// Render-thread only. Resizing/recreation imports a replacement before retiring this wrapper.
class borrowed_destination {
public:
	/// Retire owned framebuffer/depth resources, invalidating outstanding acquisition leases.
	/// No native release, swap, CPU readback or per-image GPU finish is performed.
	~borrowed_destination();
	/// Begin a trusted host acquisition with a strictly increasing nonzero generation.
	/// A wait timeout retains the existing lease instead of beginning another acquisition.
	ice::expected<acquisition_lease, diagnostic> begin_acquisition(std::uint64_t generation);
	/// Return the exact immutable extent/format checked at import.
	render_target_description description() const;

private:
	struct implementation;
	explicit borrowed_destination(std::unique_ptr<implementation> implementation);
	borrowed_destination(const borrowed_destination &)                      = delete;
	borrowed_destination           &operator=(const borrowed_destination &) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
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
/// Explicit presentation owner; ordered indexed/procedural draws share one color/depth pass.
/// UI/render thread only. Drain before a backend handoff; destroy before retiring its endpoint.
class presentation_surface {
public:
	/// Detach this presentation owner; call wait_idle before switching its native endpoint.
	~presentation_surface();
	/// Validate the entire batch, then submit it with one color/depth clear and ordered draws.
	ice::expected<frame_status, diagnostic> draw_batch(const frame_batch &batch);
	/// Set drawable pixels; zero suspends rendering. False rejects out-of-range dimensions.
	bool resize(int width, int height);
	/// Submit a procedural three-vertex draw; copy at most 16384 uniform bytes to binding zero.
	/// Foreign resources, missing required images and undersized uniforms fail before submission.
	ice::expected<frame_status, diagnostic> draw(const pipeline            &pipeline,
	                                             std::span<const std::byte> uniforms = {},
	                                             const texture *sampled_image        = nullptr);
	/// Synchronously finish preceding work for this presentation before a backend handoff.
	/// A driver failure returns a diagnostic; arbitrary driver hangs are not crash-contained.
	ice::expected<void, diagnostic> wait_idle();

private:
	struct implementation;
	explicit presentation_surface(std::unique_ptr<implementation> implementation);
	presentation_surface(const presentation_surface &other)                      = delete;
	presentation_surface           &operator=(const presentation_surface &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
};
/// Bounded indexed/procedural rendering backed by independent native implementations.
/// Owns no application/session/window. Preserve authored state outside it for reconstruction.
/// This is not yet a complete vertex-format/material/compute API. All operations use the UI/render
/// thread; GPU preparation is synchronous and can later be scheduled outside the active scene.
class device {
public:
	/// Create the requested backend or report unsupported host/build/device capabilities.
	static ice::expected<std::unique_ptr<device>, diagnostic> create(backend_kind backend);
	/// Release this owner; pipelines/surfaces retain their native device lifetime independently.
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
	/// Activate the owned EGL root context and borrow its handles for an exact native session
	/// binding. Unavailable backends return a diagnostic; caller never destroys these handles.
	ice::expected<gl::egl_binding, diagnostic> egl_context() const;
	/// Validate and wrap a native image without taking color ownership; EGL GLES3.1+ only.
	/// Exact native dimensions/internal format must match the descriptor. No implicit acquire.
	ice::expected<std::unique_ptr<borrowed_destination>, diagnostic>
	import_destination(const gl::destination_description &description);
	/// Validate device/context, live waited generation, the entire batch and texture feedback
	/// before clearing/drawing. Does not flush, swap, read back or release the native image.
	ice::expected<frame_status, diagnostic> render_into(borrowed_destination    &destination,
	                                                    const acquisition_lease &lease,
	                                                    const frame_batch       &batch);
	/// Flush submitted GL commands before the trusted adapter releases a runtime image.
	/// Does not wait for GPU completion; non-GL backends return an unsupported diagnostic.
	ice::expected<void, diagnostic> flush();
	/// Synchronously drain preceding logical-device work on the render/UI thread. Intended for
	/// detached preparation, tests, handoff or shutdown, never the live postprocess pass chain.
	/// Native failures return diagnostics; driver hangs are not contained by this call.
	ice::expected<void, diagnostic> wait_idle();
	/// Upload a validated immutable position/normal triangle list without retaining CPU aliases.
	ice::expected<std::unique_ptr<mesh>, diagnostic> create_mesh(const mesh_data &description);
	/// Upload finite position/normal/UV vertices; only position_normal_uv pipelines accept them.
	/// The CPU arrays may be changed or released after this call returns.
	ice::expected<std::unique_ptr<mesh>, diagnostic>
	create_mesh(const textured_mesh_data &description);
	/// Upload validated position/normal/UV/joints/weights vertices with an exact 64-byte layout.
	/// Joint palette sizing, animation and skinning shader math remain caller policy.
	ice::expected<std::unique_ptr<mesh>, diagnostic>
	create_mesh(const skinned_mesh_data &description);
	/// Synchronously capture an ordered indexed/procedural batch as top-row-first RGBA8.
	/// Dimensions are in [1,4096]. The whole batch is validated before touching its target.
	ice::expected<image, diagnostic> render_offscreen(const frame_batch &batch, int width,
	                                                  int height);
	/// Identify this device's backend without querying the native driver.
	backend_kind backend() const;
	/// Process-local logical identity, independent of the physical GPU and never reused.
	std::uint64_t identity() const;
	/// Return the driver/device display name for diagnostics.
	std::string name() const;
	/// Prepare a detached library/pipeline without replacing an existing pipeline on failure.
	ice::expected<std::unique_ptr<pipeline>, diagnostic>
	prepare_pipeline(const ice::io::resource_resolver &resolver, const shader_source &source,
	                 pipeline_options options = {});
	/// Prepare exactly the retained source bytes without consulting a URI handler again.
	ice::expected<std::unique_ptr<pipeline>, diagnostic>
	prepare_pipeline(const shader_snapshot &source, const std::string &vertex_entry,
	                 const std::string &fragment_entry, pipeline_options options = {});
	/// Upload one tightly packed RGBA8 image with dimensions in [1,4096].
	/// Sampling is linear/clamped without mipmaps; bind as texture(0)/sampler2D unit zero in either
	/// shader stage; draw_batch can bind it to any declared slot0..3.
	ice::expected<std::unique_ptr<texture>, diagnostic> create_texture(const image &pixels);
	/// Attach to a desktop window. The window must have no other active renderer attached;
	/// the caller keeps it alive through surface destruction.
	ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
	attach(ice::platform::desktop_window &window);
	/// Attach a platform-owned endpoint. This first native-surface adapter accepts a Metal
	/// CAMetalLayer descriptor and rejects a layer already leased by an ICE renderer.
	/// Its owner outlives this surface; unsupported API/backend pairs return a diagnostic.
	ice::expected<std::unique_ptr<presentation_surface>, diagnostic>
	attach(const ice::platform::surface &target);
	/// Synchronously capture a procedural draw as top-row-first RGBA8, with [1,4096] dimensions.
	ice::expected<image, diagnostic> render_offscreen(const pipeline &pipeline, int width,
	                                                  int                        height,
	                                                  std::span<const std::byte> uniforms = {},
	                                                  const texture *sampled_image = nullptr);
	/// Consume a bounded backend error report, or an empty string when none is pending.
	std::string take_error();

private:
	template <typename mesh_description>
	ice::expected<std::unique_ptr<mesh>, diagnostic>
	upload_mesh(const mesh_description &description, vertex_layout layout);
	struct implementation;
	explicit device(std::unique_ptr<implementation> implementation);
	device(const device &other)                                    = delete;
	device                         &operator=(const device &other) = delete;
	std::unique_ptr<implementation> implementation_;
};
} // namespace ice::render
