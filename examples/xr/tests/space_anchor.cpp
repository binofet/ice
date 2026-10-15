#include "../space_anchor.h"
#include <cmath>
#include <cstdio>

#define CHECK(condition)                                                                           \
	do {                                                                                           \
		if (!(condition)) {                                                                        \
			std::fprintf(stderr, "XR anchor failed at line %d: %s\n", __LINE__, #condition);       \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)

static bool close(const glm::mat4 &left, const glm::mat4 &right) {
	for (int column = 0; column < 4; ++column)
		for (int row = 0; row < 4; ++row)
			if (std::fabs(left[column][row] - right[column][row]) > .00001f)
				return false;
	return true;
}

int main() {
	using ice::examples::xr::space_anchor;
	space_anchor anchor;
	const auto   room  = glm::rotate(glm::translate(glm::mat4(1), glm::vec3(-1.6f, 0, -.7f)), 1.48f,
	                                 glm::vec3(0, 1, 0));
	const auto   first = glm::rotate(glm::translate(glm::mat4(1), glm::vec3(.5f, .1f, -.3f)), .35f,
	                                 glm::vec3(0, 1, 0));
	const auto   second = glm::rotate(glm::translate(glm::mat4(1), glm::vec3(-.2f, 0, .8f)), -.7f,
	                                  glm::vec3(0, 1, 0));
	// Changes before initial placement must not contaminate a fresh tracked pose.
	CHECK(anchor.queue_change(10, true, first));
	CHECK(anchor.advance(10).applied == 0);
	anchor.initialize(room);
	anchor.initialize(glm::mat4(1));
	CHECK(close(anchor.transform(), room));
	// Queue future events out of order, then check their exact display-time boundaries.
	CHECK(anchor.queue_change(30, true, second));
	CHECK(anchor.queue_change(20, true, first));
	CHECK(anchor.advance(19).applied == 0);
	CHECK(close(anchor.transform(), room));
	CHECK(anchor.advance(20).applied == 1);
	// Map the new room back into the old physical frame: both position and orientation
	// must match. This catches the inverse/sign and multiplication-order mistakes.
	CHECK(close(first * anchor.transform(), room));
	CHECK(anchor.advance(29).applied == 0);
	CHECK(anchor.advance(30).applied == 1);
	CHECK(close(first * second * anchor.transform(), room));
	const auto preserved = anchor.transform();
	CHECK(anchor.queue_change(40, false, glm::mat4(1)));
	CHECK(anchor.advance(40).unknown == 1);
	CHECK(anchor.ready());
	CHECK(close(anchor.transform(), preserved));
	CHECK(anchor.advance(50).unknown == 0);
	for (int index = 0; index < 16; ++index)
		CHECK(anchor.queue_change(100 + index, true, glm::mat4(1)));
	CHECK(!anchor.queue_change(116, true, glm::mat4(1)));
	CHECK(anchor.advance(200).applied == 16);
	CHECK(close(anchor.transform(), preserved));
	CHECK(anchor.queue_change(201, true, glm::mat4(1)));
	CHECK(anchor.advance(201).applied == 1);
	std::puts("XR anchor passed: first placement, timed origin changes, inverse/order, unknown "
	          "origin and bounded queue");
}
