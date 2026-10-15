#include "Content/icSceneAnimation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
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
icSceneMatrix compose(const std::array<float, 3> &t, std::array<float, 4> q,
                      const std::array<float, 3> &s) {
    normalize(q);
    float x = q[0], y = q[1], z = q[2], w = q[3];
    auto m = icSceneIdentity();
    m[0] = (1 - 2 * y * y - 2 * z * z) * s[0];
    m[1] = (2 * x * y + 2 * z * w) * s[0];
    m[2] = (2 * x * z - 2 * y * w) * s[0];
    m[4] = (2 * x * y - 2 * z * w) * s[1];
    m[5] = (1 - 2 * x * x - 2 * z * z) * s[1];
    m[6] = (2 * y * z + 2 * x * w) * s[1];
    m[8] = (2 * x * z + 2 * y * w) * s[2];
    m[9] = (2 * y * z - 2 * x * w) * s[2];
    m[10] = (1 - 2 * x * x - 2 * y * y) * s[2];
    std::copy(t.begin(), t.end(), m.begin() + 12);
    return m;
}
std::array<float, 4> sample(const icSceneAnimationTrack &track, float time) {
    unsigned n = track.path == icAnimationPath::Rotation ? 4 : 3;
    auto &times = track.times;
    auto &values = track.values;
    unsigned stride = n * (track.interpolation == icAnimationInterpolation::CubicSpline ? 3 : 1);
    require(!times.empty() && values.size() == times.size() * stride, "Invalid animation key data");
    auto end = std::upper_bound(times.begin(), times.end(), time);
    std::size_t a = end == times.begin() ? 0 : std::size_t(end - times.begin() - 1),
                b = std::min(a + 1, times.size() - 1);
    float dt = times[b] - times[a],
          u = dt > 0 ? std::max(0.f, std::min(1.f, (time - times[a]) / dt)) : 0;
    unsigned offset = track.interpolation == icAnimationInterpolation::CubicSpline ? n : 0;
    const float *va = values.data() + a * stride + offset;
    const float *vb = values.data() + b * stride + offset;
    std::array<float, 4> out{{0, 0, 0, 1}};
    if (track.interpolation == icAnimationInterpolation::Step || a == b || time <= times.front()) {
        std::copy(va, va + n, out.begin());
    } else if (track.interpolation == icAnimationInterpolation::CubicSpline) {
        float u2 = u * u, u3 = u2 * u;
        for (unsigned k = 0; k < n; k++)
            out[k] = (2 * u3 - 3 * u2 + 1) * va[k] + (u3 - 2 * u2 + u) * dt * va[n + k] +
                     (-2 * u3 + 3 * u2) * vb[k] + (u3 - u2) * dt * vb[int(k) - int(n)];
    } else if (track.path == icAnimationPath::Rotation) {
        std::array<float, 4> qa, qb;
        std::copy(va, va + 4, qa.begin());
        std::copy(vb, vb + 4, qb.begin());
        normalize(qa);
        normalize(qb);
        float dot = 0;
        for (unsigned k = 0; k < 4; k++)
            dot += qa[k] * qb[k];
        if (dot < 0) {
            for (auto &v : qb)
                v = -v;
            dot = -dot;
        }
        dot = std::min(1.f, dot);
        float wa = 1 - u, wb = u;
        if (dot < .9995f) {
            float angle = std::acos(dot), sn = std::sin(angle);
            wa = std::sin((1 - u) * angle) / sn;
            wb = std::sin(u * angle) / sn;
        }
        for (unsigned k = 0; k < 4; k++)
            out[k] = wa * qa[k] + wb * qb[k];
    } else
        for (unsigned k = 0; k < n; k++)
            out[k] = va[k] * (1 - u) + vb[k] * u;
    if (track.path == icAnimationPath::Rotation)
        normalize(out);
    return out;
}
} // namespace
icSceneMatrix icSceneIdentity() { return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; }
icSceneMatrix icSceneMultiply(const icSceneMatrix &a, const icSceneMatrix &b) {
    icSceneMatrix out{};
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            for (int k = 0; k < 4; k++)
                out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    return out;
}
bool icEvaluateScenePose(const icSceneAsset &scene, int clip, double seconds, bool loop,
                         icScenePose &output, std::string &diagnostics) {
    diagnostics.clear();
    try {
        require(std::isfinite(seconds), "Non-finite animation time");
        require(clip >= -1 && clip < int(scene.animations.size()), "Animation clip outside asset");
        struct TRS {
            std::array<float, 3> t, s;
            std::array<float, 4> r;
        };
        std::vector<TRS> trs;
        for (auto &node : scene.nodes)
            trs.push_back({node.translation, node.scale, node.rotation});
        if (clip >= 0) {
            auto &animation = scene.animations[clip];
            double length = double(animation.end) - animation.start;
            double t = seconds;
            if (loop && length > 0) {
                t = std::fmod(t, length);
                if (t < 0)
                    t += length;
            } else
                t = std::max(0., std::min(length, t));
            float time = float(t + animation.start);
            for (auto &track : animation.tracks) {
                require(track.node < trs.size(), "Animation target outside asset");
                auto value = sample(track, time);
                auto &dst = trs[track.node];
                require(!scene.nodes[track.node].matrixTransform,
                        "Animation targets a matrix node");
                if (track.path == icAnimationPath::Rotation)
                    dst.r = value;
                else if (track.path == icAnimationPath::Translation)
                    std::copy(value.begin(), value.begin() + 3, dst.t.begin());
                else
                    std::copy(value.begin(), value.begin() + 3, dst.s.begin());
            }
        }
        icScenePose next;
        next.local.resize(scene.nodes.size());
        next.world.resize(scene.nodes.size());
        std::vector<unsigned> pending;
        std::vector<unsigned char> done(scene.nodes.size());
        for (unsigned i = 0; i < scene.nodes.size(); i++) {
            auto &node = scene.nodes[i];
            auto &value = trs[i];
            next.local[i] = node.matrixTransform ? node.local : compose(value.t, value.r, value.s);
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
            next.world[i] = node.parent < 0
                                ? next.local[i]
                                : icSceneMultiply(next.world[node.parent], next.local[i]);
            for (float f : next.world[i])
                require(std::isfinite(f), "Non-finite pose transform");
            pending.insert(pending.end(), node.children.begin(), node.children.end());
        }
        require(visited == scene.nodes.size(), "Cycle or disconnected pose hierarchy");
        for (auto &skin : scene.skins) {
            require(skin.joints.size() == skin.inverseBind.size(), "Invalid inverse bind count");
            std::vector<icSceneMatrix> matrices;
            for (unsigned j = 0; j < skin.joints.size(); j++) {
                require(skin.joints[j] < next.world.size(), "Skin joint outside pose");
                matrices.push_back(
                    icSceneMultiply(next.world[skin.joints[j]], skin.inverseBind[j]));
                for (float f : matrices.back())
                    require(std::isfinite(f), "Non-finite skin palette");
            }
            next.palettes.push_back(std::move(matrices));
        }
        output = std::move(next);
        return true;
    } catch (const std::exception &e) {
        diagnostics = e.what();
        return false;
    }
}
bool icScenePoseBounds(const icSceneAsset &scene, const icScenePose &pose, float minimum[3],
                       float maximum[3], std::string &diagnostics) {
    diagnostics.clear();
    try {
        require(pose.world.size() == scene.nodes.size() &&
                    pose.palettes.size() == scene.skins.size(),
                "Pose does not match asset");
        bool first = true;
        float low[3] = {}, high[3] = {};
        for (auto &instance : scene.instances) {
            auto &node = scene.nodes[instance.node];
            for (auto &v : scene.primitives[instance.primitive].vertices) {
                auto m = pose.world[instance.node];
                if (node.skin >= 0) {
                    m = {};
                    auto &palette = pose.palettes[node.skin];
                    for (unsigned j = 0; j < 4; j++) {
                        require(v.joints[j] >= 0 && v.joints[j] < palette.size(),
                                "Joint index outside skin");
                        auto &joint = palette[unsigned(v.joints[j])];
                        for (unsigned k = 0; k < 16; k++)
                            m[k] += joint[k] * v.weights[j];
                    }
                }
                for (unsigned k = 0; k < 3; k++) {
                    float p = m[k] * v.position[0] + m[4 + k] * v.position[1] +
                              m[8 + k] * v.position[2] + m[12 + k];
                    require(std::isfinite(p), "Non-finite posed vertex");
                    if (first)
                        low[k] = high[k] = p;
                    else {
                        low[k] = std::min(low[k], p);
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
