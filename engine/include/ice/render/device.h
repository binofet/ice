#pragma once
#include "ice/core/expected.h"
#include "ice/io/resource_uri.h"
#include "ice/platform/desktop_window.h"
#include "ice/platform/host.h"
#include "ice/render/types.h"
#include <memory>
#include <span>
namespace ice::render {
/// Explicit backend selection. Availability depends on the host/build; failures are diagnostics.
enum class backend_kind { metal, opengl };
/// A backend-specific URI library and its procedural vertex/fragment entry names.
/// Authored shader packages may retain one variant per backend; no implicit source translation
/// occurs.
struct shader_source {
	/// Library identity resolved through configured handlers/mounts.
	std::string uri;
	/// Procedural vertex entry, with no vertex-buffer inputs in this first slice.
	std::string vertex_entry;
	/// Fragment entry, with an optional read-only buffer(0)/std140 uniform block.
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
	pipeline(const pipeline &other)                                  = delete;
	pipeline                       &operator=(const pipeline &other) = delete;
	std::unique_ptr<implementation> implementation_;
	friend class device;
	friend class presentation_surface;
};
/// Explicit presentation owner, independent of the native window and simulation. UI/render thread
/// only; drain before a backend handoff, then destroy before the native endpoint is retired.
class presentation_surface {
public:
	/// Detach this presentation owner; call wait_idle before switching its native endpoint.
	~presentation_surface();
	/// Set drawable pixels; zero suspends rendering. False rejects out-of-range dimensions.
	bool resize(int width, int height);
	/// Submit a procedural three-vertex draw; copy at most 4096 uniform bytes to binding zero.
	/// Foreign-device pipelines and undersized uniform blocks fail before submission.
	ice::expected<frame_status, diagnostic> draw(const pipeline            &pipeline,
	                                             std::span<const std::byte> uniforms = {});
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
/// Small portable procedural-rendering slice backed by independent native implementations.
/// Owns no application/session/window. Preserve authored state outside it for reconstruction.
/// This is not yet a complete mesh/texture/compute API. All operations belong on the UI/render
/// thread; GPU preparation is synchronous and can later be scheduled outside the active scene.
class device {
public:
	/// Create the requested backend or report unsupported host/build/device capabilities.
	static ice::expected<std::unique_ptr<device>, diagnostic> create(backend_kind backend);
	/// Release this owner; pipelines/surfaces retain their native device lifetime independently.
	~device();
	/// Identify this device's backend without querying the native driver.
	backend_kind backend() const;
	/// Return the driver/device display name for diagnostics.
	std::string name() const;
	/// Prepare a detached library/pipeline without replacing an existing pipeline on failure.
	ice::expected<std::unique_ptr<pipeline>, diagnostic>
	prepare_pipeline(const ice::io::resource_resolver &resolver, const shader_source &source);
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
	                                                  std::span<const std::byte> uniforms = {});
	/// Consume a bounded backend error report, or an empty string when none is pending.
	std::string take_error();

private:
	struct implementation;
	explicit device(std::unique_ptr<implementation> implementation);
	device(const device &other)                                    = delete;
	device                         &operator=(const device &other) = delete;
	std::unique_ptr<implementation> implementation_;
};
} // namespace ice::render
