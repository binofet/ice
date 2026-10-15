#include "ice/animation/scene_animation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ice::animation {
namespace {
void require(bool ok, const char *message) {
	if (!ok)
		throw std::runtime_error(message);
}
void normalize(std::array<float, 4> &q) {
	double n = 0;
	for (float v : q)
		n += double(v) * v;
	require(n > 1e-20 && std::isfinite(n), "Invalid animation quaternion");
	for (float &v : q)
		v = float(v / std::sqrt(n));
}
std::array<float, 4> slerp(std::array<float, 4> a, std::array<float, 4> b, float u) {
	if (u == 0)
		return a;
	if (u == 1)
		return b;
	normalize(a);
	normalize(b);
	// Serialized glTF quaternions are XYZW; make_quat reads that storage order.
	auto rotation =
	    glm::normalize(glm::slerp(glm::make_quat(a.data()), glm::make_quat(b.data()), u));
	std::array<float, 4> result;
	std::copy_n(glm::value_ptr(rotation), 4, result.begin());
	return result;
}
ice::math::mat4 compose(const std::array<float, 3> &t, std::array<float, 4> q,
                        const std::array<float, 3> &s) {
	normalize(q);
	return glm::translate(ice::math::mat4(1), glm::make_vec3(t.data())) *
	       glm::mat4_cast(glm::make_quat(q.data())) *
	       glm::scale(ice::math::mat4(1), glm::make_vec3(s.data()));
}
std::array<float, 4> sample(const ice::assets::scene_animation_track &track, float time) {
	unsigned n      = track.path == ice::assets::animation_path::rotation ? 4 : 3;
	auto    &times  = track.times;
	auto    &values = track.values;
	unsigned stride =
	    n * (track.interpolation == ice::assets::animation_interpolation::cubic_spline ? 3 : 1);
	require(!times.empty() && values.size() == times.size() * stride, "Invalid animation key data");
	auto        end = std::upper_bound(times.begin(), times.end(), time);
	std::size_t a   = end == times.begin() ? 0 : std::size_t(end - times.begin() - 1),
	            b   = std::min(a + 1, times.size() - 1);
	float dt        = times[b] - times[a],
	      u         = dt > 0 ? std::max(0.f, std::min(1.f, (time - times[a]) / dt)) : 0;
	unsigned offset =
	    track.interpolation == ice::assets::animation_interpolation::cubic_spline ? n : 0;
	const float         *va = values.data() + a * stride + offset;
	const float         *vb = values.data() + b * stride + offset;
	std::array<float, 4> out{{0, 0, 0, 1}};
	if (track.interpolation == ice::assets::animation_interpolation::step || a == b ||
	    time <= times.front()) {
		std::copy(va, va + n, out.begin());
	} else if (track.interpolation == ice::assets::animation_interpolation::cubic_spline) {
		float u2 = u * u, u3 = u2 * u;
		for (unsigned k = 0; k < n; k++)
			out[k] = (2 * u3 - 3 * u2 + 1) * va[k] + (u3 - 2 * u2 + u) * dt * va[n + k] +
			         (-2 * u3 + 3 * u2) * vb[k] + (u3 - u2) * dt * vb[int(k) - int(n)];
	} else if (track.path == ice::assets::animation_path::rotation) {
		std::array<float, 4> qa, qb;
		std::copy(va, va + 4, qa.begin());
		std::copy(vb, vb + 4, qb.begin());
		out = slerp(qa, qb, u);
	} else
		for (unsigned k = 0; k < n; k++)
			out[k] = va[k] * (1 - u) + vb[k] * u;
	if (track.path == ice::assets::animation_path::rotation)
		normalize(out);
	return out;
}
pose build_pose(const ice::assets::scene_asset &scene, std::vector<transform> trs) {
	pose next;
	next.transforms = std::move(trs);
	next.local.resize(scene.nodes.size());
	next.world.resize(scene.nodes.size());
	std::vector<unsigned>      pending;
	std::vector<unsigned char> done(scene.nodes.size());
	for (unsigned i = 0; i < scene.nodes.size(); i++) {
		auto &node    = scene.nodes[i];
		auto &value   = next.transforms[i];
		next.local[i] = node.matrix_transform
		                    ? node.local
		                    : compose(value.translation, value.rotation, value.scale);
		if (node.parent < 0)
			pending.push_back(i);
	}
	unsigned visited = 0;
	while (!pending.empty()) {
		auto i = pending.back();
		pending.pop_back();
		require(i < scene.nodes.size() && !done[i], "Invalid pose hierarchy");
		done[i] = 1;
		++visited;
		auto &node = scene.nodes[i];
		require(node.parent < 0 || (unsigned(node.parent) < done.size() && done[node.parent]),
		        "Invalid pose parent");
		next.world[i] = node.parent < 0 ? next.local[i]
		                                : multiply_matrices(next.world[node.parent], next.local[i]);
		for (float f : ice::math::elements(next.world[i]))
			require(std::isfinite(f), "Non-finite pose transform");
		pending.insert(pending.end(), node.children.begin(), node.children.end());
	}
	require(visited == scene.nodes.size(), "Cycle or disconnected pose hierarchy");
	for (auto &skin : scene.skins) {
		require(skin.joints.size() == skin.inverse_bind.size(), "Invalid inverse bind count");
		std::vector<ice::math::mat4> matrices;
		for (unsigned j = 0; j < skin.joints.size(); j++) {
			require(skin.joints[j] < next.world.size(), "Skin joint outside pose");
			matrices.push_back(multiply_matrices(next.world[skin.joints[j]], skin.inverse_bind[j]));
			for (float f : ice::math::elements(matrices.back()))
				require(std::isfinite(f), "Non-finite skin palette");
		}
		next.palettes.push_back(std::move(matrices));
	}
	return next;
}
} // namespace
ice::math::mat4 identity_matrix() { return ice::math::mat4(1); }
ice::math::mat4 multiply_matrices(const ice::math::mat4 &left, const ice::math::mat4 &right) {
	return left * right;
}
bool evaluate_pose(const ice::assets::scene_asset &scene, int clip, double seconds, bool loop,
                   pose &output, std::string &diagnostics) {
	diagnostics.clear();
	try {
		require(std::isfinite(seconds), "Non-finite animation time");
		require(clip >= -1 && clip < int(scene.animations.size()), "Animation clip outside asset");
		std::vector<transform> trs;
		for (auto &node : scene.nodes) {
			transform t;
			t.translation = node.translation;
			t.scale       = node.scale;
			t.rotation    = node.rotation;
			trs.push_back(t);
		}
		if (clip >= 0) {
			auto  &animation = scene.animations[clip];
			double length    = double(animation.end) - animation.start;
			double t         = seconds;
			if (loop && length > 0) {
				t = std::fmod(t, length);
				if (t < 0)
					t += length;
			} else
				t = std::max(0., std::min(length, t));
			float time = float(t + animation.start);
			for (auto &track : animation.tracks) {
				require(track.node < trs.size(), "Animation target outside asset");
				auto  value = sample(track, time);
				auto &dst   = trs[track.node];
				require(!scene.nodes[track.node].matrix_transform,
				        "Animation targets a matrix node");
				if (track.path == ice::assets::animation_path::rotation)
					dst.rotation = value;
				else if (track.path == ice::assets::animation_path::translation)
					std::copy(value.begin(), value.begin() + 3, dst.translation.begin());
				else
					std::copy(value.begin(), value.begin() + 3, dst.scale.begin());
			}
		}
		auto next = build_pose(scene, std::move(trs));
		output    = std::move(next);
		return true;
	} catch (const std::exception &e) {
		diagnostics = e.what();
		return false;
	}
}
bool blend_poses(const ice::assets::scene_asset &scene, const pose &from, const pose &to,
                 float weight, pose &output, std::string &diagnostics) {
	diagnostics.clear();
	try {
		require(std::isfinite(weight) && weight >= 0 && weight <= 1,
		        "Blend weight must be in [0,1]");
		require(from.transforms.size() == scene.nodes.size() &&
		            to.transforms.size() == scene.nodes.size(),
		        "Blend poses do not match asset");
		auto trs = from.transforms;
		for (unsigned i = 0; i < trs.size(); i++) {
			if (scene.nodes[i].matrix_transform)
				continue;
			auto &a     = from.transforms[i];
			auto &b     = to.transforms[i];
			auto &value = trs[i];
			for (unsigned k = 0; k < 3; k++) {
				value.translation[k] = a.translation[k] * (1 - weight) + b.translation[k] * weight;
				value.scale[k]       = a.scale[k] * (1 - weight) + b.scale[k] * weight;
			}
			value.rotation = slerp(a.rotation, b.rotation, weight);
		}
		auto next = build_pose(scene, std::move(trs));
		output    = std::move(next);
		return true;
	} catch (const std::exception &e) {
		diagnostics = e.what();
		return false;
	}
}
bool scene_pose_bounds(const ice::assets::scene_asset &scene, const pose &evaluated_pose,
                       float minimum[3], float maximum[3], std::string &diagnostics) {
	diagnostics.clear();
	try {
		require(evaluated_pose.world.size() == scene.nodes.size() &&
		            evaluated_pose.palettes.size() == scene.skins.size(),
		        "Pose does not match asset");
		bool  first  = true;
		float low[3] = {}, high[3] = {};
		for (auto &instance : scene.instances) {
			auto &node = scene.nodes[instance.node];
			for (auto &v : scene.primitives[instance.primitive].vertices) {
				auto m = evaluated_pose.world[instance.node];
				if (node.skin >= 0) {
					m             = ice::math::mat4(0);
					auto &palette = evaluated_pose.palettes[node.skin];
					for (unsigned j = 0; j < 4; j++) {
						require(v.joints[j] >= 0 && v.joints[j] < palette.size(),
						        "Joint index outside skin");
						auto &joint = palette[unsigned(v.joints[j])];
						for (unsigned k = 0; k < 16; k++)
							ice::math::element(m, k) += ice::math::element(joint, k) * v.weights[j];
					}
				}
				for (unsigned k = 0; k < 3; k++) {
					float p = ice::math::element(m, k) * v.position[0] +
					          ice::math::element(m, 4 + k) * v.position[1] +
					          ice::math::element(m, 8 + k) * v.position[2] +
					          ice::math::element(m, 12 + k);
					require(std::isfinite(p), "Non-finite posed vertex");
					if (first)
						low[k] = high[k] = p;
					else {
						low[k]  = std::min(low[k], p);
						high[k] = std::max(high[k], p);
					}
				}
				first = false;
			}
		}
		require(!first, "No posed geometry");
		std::copy(low, low + 3, minimum);
		std::copy(high, high + 3, maximum);
		return true;
	} catch (const std::exception &e) {
		diagnostics = e.what();
		return false;
	}
}

} // namespace ice::animation
