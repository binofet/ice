#pragma once
#include "ice/render/acquisition_lease.h"

namespace ice::render::detail {
struct acquisition_lifetime {};
struct acquisition_state {
	std::weak_ptr<const acquisition_lifetime> owner;
	std::uint64_t                             generation = 0;
	bool                                      active = true, waited = false;
};
// One source per immutable destination; unique lifetime identity prevents pointer-address reuse.
class lease_source {
public:
	lease_source()                                                               = default;
	lease_source(const lease_source &)                                           = delete;
	lease_source                                &operator=(const lease_source &) = delete;
	ice::expected<acquisition_lease, diagnostic> acquire(std::uint64_t generation);
	ice::expected<void, diagnostic>              validate(const acquisition_lease &lease) const;

private:
	std::shared_ptr<const acquisition_lifetime> owner_ =
	    std::make_shared<const acquisition_lifetime>();
	std::weak_ptr<acquisition_state> active_;
	std::uint64_t                    latest_ = 0;
};
} // namespace ice::render::detail
