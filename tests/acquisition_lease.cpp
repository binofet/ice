#include "../engine/src/render/lease_state.h"
#include <cstdio>
#include <limits>
#include <optional>
#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "Lease check %d: %s\n", __LINE__, #condition);                    \
			return 1;                                                                              \
		}                                                                                          \
	} while (false)
int main() {
	using namespace ice::render;
	detail::lease_source images, foreign;
	CHECK(!images.acquire(0));
	auto first = images.acquire(1);
	CHECK(first && first->active() && !first->waited());
	CHECK(!images.validate(*first) && !images.acquire(2));
	CHECK(first->generation() == 1); // A timeout keeps exactly the same pending acquisition.
	CHECK(first->mark_waited() && first->mark_waited() && images.validate(*first));
	CHECK(!foreign.validate(*first));
	auto moved = std::move(*first);
	CHECK(!first->active() && moved.waited());
	moved.expire();
	CHECK(!moved.active() && !moved.mark_waited() && !images.validate(moved));
	CHECK(!images.acquire(1));
	{
		auto second = images.acquire(2);
		CHECK(second && second->mark_waited());
	}
	auto third = images.acquire(3);
	CHECK(third && third->mark_waited());
	acquisition_lease retained;
	{
		detail::lease_source temporary;
		auto                 candidate = temporary.acquire(1);
		CHECK(candidate && candidate->mark_waited());
		retained = std::move(*candidate);
		CHECK(retained.waited());
	}
	CHECK(!retained.active() && !retained.mark_waited());
	third->expire();
	auto last = images.acquire(std::numeric_limits<std::uint64_t>::max());
	CHECK(last);
	last->expire();
	CHECK(!images.acquire(1) && !images.acquire(0));
	std::puts("Image leases: timeout retention, foreign/generation checks, move/expiry, lifetime "
	          "retirement and exhaustion passed");
}
