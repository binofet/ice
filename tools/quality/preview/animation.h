#pragma once

#include <array>
#include <chrono>
#include <string>
#include <vector>

// Design preview only. This header is not connected to the engine build yet.
namespace ice::assets {
/// @brief Immutable scene geometry, bindings and animation clips loaded through a URI resolver.
struct scene_asset;
} // namespace ice::assets

namespace ice::animation {
/// @brief Animation time expressed in seconds, independently of the render frame rate.
using seconds = std::chrono::duration<double>;

/// @brief A column-major, right-handed 4x4 transform; translations are in scene units.
using matrix = std::array<float, 16>;

/// @brief Local transform components used before evaluating parents and skin bindings.
/// @details Animation blending operates on these components, never on final matrices.
struct transform {
	/// @brief Translation relative to the parent, in scene units.
	std::array<float, 3> translation = {0, 0, 0};
	/// @brief Quaternion rotation in XYZW order; evaluated rotations are normalized.
	std::array<float, 4> rotation = {0, 0, 0, 1};
	/// @brief Dimensionless scale relative to the parent.
	std::array<float, 3> scale = {1, 1, 1};
};

/// @brief Mutable pose for one scene instance; it does not own its source asset.
/// @details Public data records use plain field names. Private class state uses a trailing
/// underscore.
/// @note A pose must only be consumed with the asset from which it was evaluated.
struct pose {
	/// @brief Sampled local components in asset node order.
	std::vector<transform> transforms;
	/// @brief Local matrices in asset node order, including authored matrix-only nodes.
	std::vector<matrix> local;
	/// @brief World-space matrices after parent evaluation, in asset node order.
	std::vector<matrix> world;
	/// @brief Per-skin world-space palettes, computed as joint world transform times inverse bind.
	std::vector<std::vector<matrix>> palettes;
};

/// @brief Sample a clip and rebuild the hierarchy and skin palettes.
/// @param asset Immutable scene supplying node transforms, skins and clips.
/// @param clip Zero-based clip index; -1 selects the authored rest pose.
/// @param time Time relative to the clip's first key; negative times are allowed.
/// @param loop Whether to wrap time; false clamps to the clip endpoints.
/// @param output Replaced only after the entire evaluation succeeds.
/// @param error Cleared on success; receives a readable diagnostic on failure.
/// @return True on success; false leaves output unchanged.
/// @note Different output poses may be evaluated concurrently against the same immutable asset.
[[nodiscard]] bool evaluate_pose(const assets::scene_asset &asset, int clip, seconds time,
                                 bool loop, pose &output, std::string &error);

/// @brief Blend local components, then rebuild the hierarchy and skin palettes.
/// @param asset Immutable scene shared by both input poses.
/// @param from Outgoing pose evaluated from asset.
/// @param to Incoming pose evaluated from asset.
/// @param weight Finite incoming weight in [0,1]; endpoints reproduce their input pose.
/// @param output Destination pose; may alias either input and is unchanged on failure.
/// @param error Cleared on success; receives a readable diagnostic on failure.
/// @return True on success; false for invalid weight, incompatible data or invalid transforms.
[[nodiscard]] bool blend_poses(const assets::scene_asset &asset, const pose &from, const pose &to,
                               float weight, pose &output, std::string &error);

/// @brief Playback settings for the inspector's next transition.
/// @details This small class demonstrates the proposed private-member convention.
/// @note Actual playback-controller ownership remains a separate implementation decision.
class transition_settings {
public:
	/// @brief Return whether the next clip change should crossfade rather than cut.
	[[nodiscard]] bool crossfade_enabled() const noexcept { return crossfade_enabled_; }

	/// @brief Enable or disable crossfading for the next clip change.
	/// @param enabled True selects a fade; false selects a hard cut.
	void set_crossfade_enabled(bool enabled) noexcept { crossfade_enabled_ = enabled; }

	/// @brief Return the active-playback duration of the next crossfade.
	[[nodiscard]] seconds duration() const noexcept { return duration_; }

private:
	bool    crossfade_enabled_ = true;
	seconds duration_          = seconds{0.6};
};
} // namespace ice::animation
