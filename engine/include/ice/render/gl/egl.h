#pragma once
#include "ice/render/types.h"
#include <cstdint>

namespace ice::render::gl {
/// The one shared GL command implementation selects desktop core or embedded GLSL/GL state.
enum class profile { desktop_core, embedded };
/// Borrowed EGL root handles for native session binding. The creating device/resources own them.
/// Native runtime sessions/images must retire before the final device/resource owner is released.
/// Opaque SDK handles avoid requiring EGL headers in ordinary portable consumers.
struct egl_binding {
	/// Native EGLDisplay, EGLConfig and EGLContext, respectively; never owned by the caller.
	void *display = nullptr, *config = nullptr, *context = nullptr;
	/// Unique logical context lifetime; native pointer reuse cannot reuse this identity.
	std::uint64_t identity = 0;
	/// Compare exact native handles and logical lifetime, not merely the physical GPU.
	bool operator==(const egl_binding &) const = default;
};
/// A trusted native adapter's single-layer, single-sample GL_TEXTURE_2D color attachment.
/// The adapter retains this image until wrapper destruction and expires its lease before release.
/// Import validates native dimensions/internal format, EGL ownership and FBO completeness.
struct destination_description {
	/// Exact binding returned by this device; another context/share group is rejected.
	egl_binding context;
	/// Native texture name; ownership is retained by the runtime, never transferred to ICE.
	std::uint32_t color_image = 0;
	/// Exact image extent, each component in[1,16384] and within native limits.
	int width = 0, height = 0;
	/// RGBA8 UNORM/sRGB or supported RGBA16F. sRGB destinations require linear shader output.
	color_format format = color_format::rgba8_unorm;
};
} // namespace ice::render::gl
