#include "readback_state.h"
namespace ice::render {
readback_ticket::readback_ticket()                                       = default;
readback_ticket::~readback_ticket()                                      = default;
readback_ticket::readback_ticket(readback_ticket &&) noexcept            = default;
readback_ticket &readback_ticket::operator=(readback_ticket &&) noexcept = default;
readback_ticket::readback_ticket(std::unique_ptr<detail::readback_operation> operation)
    : operation_(std::move(operation)) {}
readback_ticket::operator bool() const noexcept { return bool(operation_); }
ice::expected<readback_status, diagnostic> readback_ticket::poll() {
	if (!operation_)
		return ice::unexpected(diagnostic{"Empty readback ticket"});
	return operation_->poll();
}
ice::expected<readback_pixels, diagnostic> readback_ticket::take() {
	if (!operation_)
		return ice::unexpected(diagnostic{"Empty readback ticket"});
	return operation_->take();
}
void readback_ticket::cancel() noexcept {
	if (operation_)
		operation_->cancel();
}
} // namespace ice::render
