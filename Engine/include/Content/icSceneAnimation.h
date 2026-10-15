#pragma once
#include "Content/icSceneAsset.h"

// Per-instance mutable state. The asset remains immutable and shareable.
struct icScenePose {
    std::vector<icSceneMatrix> local, world;
    // World-space joint matrices: jointWorld * inverseBind. No mesh transform twice.
    std::vector<std::vector<icSceneMatrix>> palettes;
};
icSceneMatrix icSceneIdentity();
icSceneMatrix icSceneMultiply(const icSceneMatrix &, const icSceneMatrix &);
// seconds is relative to the clip start; -1 evaluates the authored rest pose.
// Loop wraps negative times too; non-looping playback clamps to both endpoints.
// Failure leaves output intact. No parser or graphics context is required.
bool icEvaluateScenePose(const icSceneAsset &, int clip, double seconds, bool loop,
                         icScenePose &output, std::string &diagnostics);
// CPU reference for inspection/bounds/tests; rendering deforms vertices on the GPU.
bool icScenePoseBounds(const icSceneAsset &, const icScenePose &, float minimum[3],
                       float maximum[3], std::string &diagnostics);
