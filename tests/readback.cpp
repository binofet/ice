#include "ice/render/device.h"
#include "readback_state.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>
using namespace ice::render;
namespace {
void check(bool condition) {
	if (!condition)
		throw std::runtime_error("Readback lifecycle check failed");
}
struct transfer {
	std::shared_ptr<detail::readback_reservation> reservation;
	bool completed = false, cancelled = false, failed = false, consumed = false;
};
class operation final : public detail::readback_operation {
	std::shared_ptr<transfer> state_;

public:
	explicit operation(std::shared_ptr<transfer> state) : state_(std::move(state)) {}
	ice::expected<readback_status, diagnostic> poll() override {
		if (state_->consumed)
			return readback_status::consumed;
		if (!state_->completed)
			return readback_status::pending;
		if (state_->cancelled)
			return readback_status::cancelled;
		if (state_->failed)
			return ice::unexpected(diagnostic{"Injected backend failure"});
		return readback_status::ready;
	}
	ice::expected<readback_pixels, diagnostic> take() override {
		const auto status = poll();
		if (!status)
			return ice::unexpected(status.error());
		if (*status != readback_status::ready)
			return ice::unexpected(diagnostic{"Not ready"});
		readback_pixels pixels;
		pixels.width      = 1;
		pixels.height     = 1;
		pixels.row_stride = 8;
		pixels.layout     = readback_layout::bgra8;
		pixels.origin     = readback_origin::bottom_left;
		pixels.bytes      = {1, 2, 3, 4, 0, 0, 0, 0};
		detail::readback_access::retain(pixels, std::move(state_->reservation));
		state_->consumed = true;
		return pixels;
	}
	void cancel() noexcept override { state_->cancelled = true; }
};
auto ticket(const std::shared_ptr<transfer> &state) {
	return detail::readback_access::ticket(std::make_unique<operation>(state));
}
void lifecycle() {
	static_assert(!std::is_copy_constructible_v<readback_ticket>);
	static_assert(std::is_nothrow_move_constructible_v<readback_ticket>);
	static_assert(!std::is_copy_constructible_v<readback_pixels>);
	static_assert(std::is_nothrow_move_assignable_v<readback_pixels>);
	readback_submission none;
	check(none.status == frame_status::busy && !none.ticket);
	readback_ticket empty;
	check(!empty && !empty.poll() && !empty.take());
	empty.cancel();
	auto accounting    = std::make_shared<detail::readback_accounting>();
	auto state         = std::make_shared<transfer>();
	state->reservation = detail::reserve_readback(accounting, {1, 64}, 64);
	check(bool(state->reservation));
	auto pending = ticket(state);
	check(*pending.poll() == readback_status::pending && !pending.take());
	auto moved = std::move(pending);
	check(!pending && bool(moved) && *moved.poll() == readback_status::pending);
	check(!detail::reserve_readback(accounting, {1, 128}, 64));
	check(!detail::reserve_readback(accounting, {2, 127}, 64));
	state->completed = true;
	check(*moved.poll() == readback_status::ready);
	auto pixels = moved.take();
	check(bool(pixels));
	check(*moved.poll() == readback_status::consumed && !moved.take());
	check(pixels->bytes.size() == 8 && pixels->row_stride == 8 &&
	      pixels->layout == readback_layout::bgra8 &&
	      pixels->origin == readback_origin::bottom_left);
	moved = {};
	state.reset();
	check(accounting->bytes == 64 && accounting->tickets == 1);
	// Writer-thread destruction must release admission safely after all pixel bytes are owned.
	std::jthread writer([owned = std::move(*pixels)] { check(owned.bytes[2] == 3); });
	writer.join();
	check(accounting->bytes == 0 && accounting->tickets == 0);
}
void cancellation() {
	auto accounting      = std::make_shared<detail::readback_accounting>();
	auto command         = std::make_shared<transfer>();
	command->reservation = detail::reserve_readback(accounting, {1, 64}, 64);
	auto pending         = ticket(command);
	pending.cancel();
	check(*pending.poll() == readback_status::pending && !pending.take());
	auto replacement = std::move(pending);
	replacement = {}; // Simulated submitted command independently owns native resource lifetime.
	check(accounting->bytes == 64 && !detail::reserve_readback(accounting, {1, 64}, 64));
	command->completed = true;
	auto cancelled     = ticket(command);
	check(*cancelled.poll() == readback_status::cancelled && !cancelled.take());
	cancelled = {};
	command.reset();
	auto next = detail::reserve_readback(accounting, {1, 64}, 64);
	check(bool(next));
	next.reset();
	check(!detail::reserve_readback(accounting, {0, 64}, 64));
	check(!detail::reserve_readback(accounting, {1, 63}, 64));
	check(!detail::reserve_readback(accounting, {1, 0}, 64));
}
void failure() {
	auto state       = std::make_shared<transfer>();
	state->completed = state->failed = true;
	auto failed                      = ticket(state);
	check(!failed.poll() && !failed.take());
	check(failed.poll().error().message == "Injected backend failure");
	failed.cancel();
	check(*failed.poll() == readback_status::cancelled);
}
} // namespace
int main(int argc, char **argv) {
	try {
		check(argc == 2);
		const std::string selected = argv[1];
		if (selected == "lifecycle")
			lifecycle();
		else if (selected == "cancellation")
			cancellation();
		else if (selected == "failure")
			failure();
		else
			throw std::runtime_error("Unknown readback fixture");
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	std::cout << "CPU-only readback " << argv[1] << " passed\n";
}
