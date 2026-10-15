#pragma once

#include "ice/math/types.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace ice::examples::xr {

// The room is placed once. Reference-space changes alter its coordinates, never its
// intended physical location. The probe's reference space has an identity origin offset.
class space_anchor {
public:
	struct change_result {
		std::size_t applied = 0, unknown = 0;
	};
	bool             ready() const { return ready_; }
	const glm::mat4 &transform() const { return transform_; }
	void             initialize(const glm::mat4 &transform) {
        if (!ready_) {
            transform_ = transform;
            ready_     = true;
        }
	}
	bool queue_change(std::int64_t time, bool valid, const glm::mat4 &old_from_new) {
		if (changes_.size() == 16)
			return false;
		changes_.push_back({time, valid, old_from_new});
		std::stable_sort(changes_.begin(), changes_.end(), [](const auto &left, const auto &right) {
			return left.time < right.time;
		});
		return true;
	}
	change_result advance(std::int64_t display_time) {
		change_result result;
		auto          end = changes_.begin();
		while (end != changes_.end() && end->time <= display_time) {
			if (ready_) {
				if (end->valid) {
					// OpenXR gives the NEW origin in OLD coordinates. Invert that mapping
					// to express the existing room in the new coordinates at changeTime.
					transform_ = glm::inverse(end->old_from_new) * transform_;
					++result.applied;
				} else {
					// No runtime-provided relationship exists. Retain coordinates and report
					// the discontinuity; never silently move the room to the current gaze.
					++result.unknown;
				}
			}
			++end;
		}
		changes_.erase(changes_.begin(), end);
		return result;
	}

private:
	struct origin_change {
		std::int64_t time;
		bool         valid;
		glm::mat4    old_from_new;
	};
	glm::mat4                  transform_{1};
	bool                       ready_ = false;
	std::vector<origin_change> changes_;
};

} // namespace ice::examples::xr
