#pragma once
#include "ice/core/expected.h"
#include "ice/render/types.h"
#include <cstdint>
#include <memory>

namespace ice::render {
namespace detail {
/// Private owned acquisition state; native runtime objects never enter this record.
struct acquisition_state;
/// Private per-destination lifetime/generation authority used by native adapters.
class lease_source;
} // namespace detail
/// Render-thread-only proof supplied by a trusted native image adapter, not an XR API validator.
/// A timeout retains this same unwaited lease/image. Mark waited only after native wait succeeds;
/// expire before native release, and retain the runtime image/context until its wrapper retires.
class acquisition_lease {
public:
	/// Construct an invalid, inactive lease, useful for optional native acquisition state.
	acquisition_lease() noexcept;
	/// Move the sole host lease owner without changing acquisition state.
	acquisition_lease(acquisition_lease &&other) noexcept;
	/// Expire this owner's previous acquisition before taking another lease.
	acquisition_lease &operator=(acquisition_lease &&other) noexcept;
	/// Expire this acquisition; this never invokes native wait/release or GPU work.
	~acquisition_lease();
	/// Attest native wait success on this still-live acquisition; repeated calls are harmless.
	ice::expected<void, diagnostic> mark_waited();
	/// Invalidate before the native image is released; idempotent, with no native side effects.
	void expire() noexcept;
	/// True while both this acquisition and its owning destination remain alive.
	bool active() const noexcept;
	/// True only for an active acquisition whose host attested successful native waiting.
	bool waited() const noexcept;
	/// Host acquisition generation, or zero for an empty/moved-from lease.
	std::uint64_t generation() const noexcept;

private:
	explicit acquisition_lease(std::shared_ptr<detail::acquisition_state> state) noexcept;
	acquisition_lease(const acquisition_lease &)                                    = delete;
	acquisition_lease                         &operator=(const acquisition_lease &) = delete;
	std::shared_ptr<detail::acquisition_state> state_;
	friend class detail::lease_source;
};
} // namespace ice::render
