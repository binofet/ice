#pragma once
#include <cstddef>
#include <deque>
#include <utility>

namespace playground {
// Deliberately different operations/storage from std::vector: the adapter must do real work.
template <typename T> class custom_vector {
public:
	void                             add(T value) { elements_.push_back(std::move(value)); }
	std::size_t                      count() const { return elements_.size(); }
	template <typename Visitor> auto each(Visitor &&visitor) const {
		for (const auto &value : elements_) {
			auto checked = visitor(value);
			if (!checked)
				return checked;
		}
		return decltype(visitor(std::declval<const T &>())){};
	}
	bool operator==(const custom_vector &) const = default;

private:
	std::deque<T> elements_;
};
} // namespace playground
