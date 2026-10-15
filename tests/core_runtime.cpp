#include "ice/ai/a_star.h"
#include "ice/input/input.h"
#include "ice/io/file.h"
#include "ice/memory/allocation_debug.h"
#include "ice/memory/min_heap.h"
#include "ice/memory/system_memory_pool.h"
#include "ice/platform/platform_events.h"
#include "ice/util/settings_file.h"
#include "ice/util/state_machine.h"
#include "ice/util/time.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unistd.h>
#define CHECK(x)                                                                                   \
	do {                                                                                           \
		if (!(x)) {                                                                                \
			std::fprintf(stderr, "failed line %d: %s\n", __LINE__, #x);                            \
			return 1;                                                                              \
		}                                                                                          \
	} while (0)
struct TestState : ice::util::state {
	int         pops = 0;
	ice::result update(float, bool *) override { return IC_OK; }
	ice::result render() override { return IC_OK; }
	void        on_push() override {}
	void        on_pop() override { ++pops; }
};
int main() {
	// Exercise allocation entry points directly: accidental unqualified calls recurse.
	ice::memory::system_memory_pool pool(0);
	auto                           *bytes = static_cast<unsigned char *>(pool.calloc(8, 1));
	CHECK(bytes);
	for (int i = 0; i < 8; ++i)
		CHECK(bytes[i] == 0);
	bytes[0]    = 42;
	auto *grown = static_cast<unsigned char *>(pool.realloc(bytes, 32));
	CHECK(grown && grown[0] == 42);
	pool.free(grown);
	auto *block = pool.malloc(16);
	CHECK(block);
	pool.free(block);
#ifndef _WIN32
	auto *aligned = pool.memalign(64, 17);
	CHECK(aligned && reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0);
	pool.free(aligned);
#endif
	CHECK(pool.memalign(3, 16) == nullptr);
	void                                 *frames[] = {reinterpret_cast<void *>(std::uintptr_t(1))};
	ice::memory::debug::stack_trace       trace(7, 1, frames);
	ice::memory::debug::allocation_record record(17, 32, frames, &pool, &trace);
	CHECK(trace.hash == 7 && trace.size == 1 && trace.frames == frames);
	CHECK(record.requested == 17 && record.actual == 32 && record.address == frames &&
	      record.pool == &pool && record.trace == &trace);
	int heap[] = {8, 3, 5, 1};
	ice::memory::min_heap_build(heap, 0);
	ice::memory::min_heap_build(heap, 4);
	CHECK(ice::memory::min_heap_dequeue(heap, 4) == 1);

	ice::platform::platform_event event;
	while (ice::platform::platform_events::poll(event)) {
	}
	auto firstID  = ice::platform::platform_events::allocate_window_id(),
	     secondID = ice::platform::platform_events::allocate_window_id();
	CHECK(firstID != 0 && secondID > firstID);
	ice::platform::platform_events::post(ice::platform::platform_event_type::close_requested,
	                                     secondID);
	ice::platform::platform_events::post(
	    ice::platform::platform_event_type::application_quit_requested);
	CHECK(ice::platform::platform_events::poll(event) && event.window == secondID);
	auto sequence = event.sequence;
	CHECK(ice::platform::platform_events::poll(event) &&
	      event.window == ice::platform::ic_application_scope && event.sequence > sequence);
	CHECK(!ice::platform::platform_events::poll(event));
	CHECK(ICEFAIL(IC_FAIL_GEN));
	CHECK(!ICEFAIL(IC_OK));
	char path[] = "/tmp/ice-core-XXXXXX";
	int  fd     = mkstemp(path);
	CHECK(fd >= 0);
	close(fd);
	ice::io::file file;
	size_t        actual    = 0;
	char          payload[] = "preserve";
	CHECK(!ICEFAIL(file.open(path, ICFMREAD_WRITE_NEW)));
	CHECK(!ICEFAIL(file.write(payload, 8, &actual)) && actual == 8);
	CHECK(!ICEFAIL(file.set_pos(3)));
	ice::uint64 size = 0, pos = 0;
	CHECK(!ICEFAIL(file.get_size(&size)) && size == 8);
	CHECK(!ICEFAIL(file.get_pos(&pos)) && pos == 3);
	file.close();
	CHECK(!ICEFAIL(file.open(path, ICFMREAD_CREATE)));
	char data[16] = {};
	CHECK(ICEFAIL(file.read(data, 16, &actual)) && actual == 8);
	CHECK(std::strcmp(data, payload) == 0);
	file.close();
	FILE *f = std::fopen(path, "wb");
	CHECK(f);
	std::fputs("[Engine.Graphics]\r\nScreenX = 1280\r\n", f);
	std::fclose(f);
	ice::util::settings_file settings;
	CHECK(!ICEFAIL(settings.read_file(path)));
	CHECK(std::strcmp(settings.get_value("Engine.Graphics.ScreenX").c_str(), "1280") == 0);
	CHECK(!ICEFAIL(settings.add_pair("Engine.Graphics.ScreenX", "800")));
	CHECK(!ICEFAIL(settings.write_file(path)));
	ice::util::settings_file restored;
	CHECK(!ICEFAIL(restored.read_file(path)));
	CHECK(std::strcmp(restored.get_value("Engine.Graphics.ScreenX").c_str(), "800") == 0);
	unlink(path);
	ice::ai::a_star grid;
	CHECK(!ICEFAIL(grid.create(8, 4, 0xff)));
	ice::math::vec3   point;
	ice::uint         index;
	ice::ai::pathnode node;
	CHECK(grid.try_get_world_position(19, point) && point.x == 3 && point.y == 2);
	CHECK(grid.try_get_node_index(point, &index) && index == 19);
	CHECK(grid.try_get_node(point, &node) && node == grid.get_node(3, 2));
	CHECK(!grid.try_get_node_index(ice::math::vec3(-1, 0, 0), &index));
	CHECK(!grid.try_get_node_index(ice::math::vec3(8, 0, 0), &index));
	CHECK(!grid.try_get_world_position(32, point));
	CHECK(!ICEFAIL(grid.save(path)));
	CHECK(!ICEFAIL(grid.load(path)));
	FILE *pathFile = fopen(path, "rb");
	CHECK(pathFile);
	fseek(pathFile, 0, SEEK_END);
	CHECK(ftell(pathFile) == 40);
	fclose(pathFile);
	pathFile = fopen(path, "wb");
	CHECK(pathFile);
	fputs("bad", pathFile);
	fclose(pathFile);
	CHECK(ICEFAIL(grid.load(path)));
	unlink(path);
	ice::util::timer timer;
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	auto elapsed = timer.elapsed();
	CHECK(elapsed >= 0.01 && elapsed < 1);
	auto input = ice::input::input::instance();
	input->set_down(ice::input::key_a, true);
	CHECK(input->is_down(ice::input::key_a));
	input->update();
	input->set_down(ice::input::key_a, false);
	CHECK(input->is_pressed(ice::input::key_a));
	input->update();
	CHECK(!input->is_pressed(ice::input::key_a));
	input->set_down(ice::input::mouse_l, true);
	input->update();
	input->cancel_held_input();
	CHECK(!input->is_down(ice::input::mouse_l) && !input->is_pressed(ice::input::mouse_l));
	input->hide_curs();
	input->update_mouse_position(100000, 100000, 720);
	input->update();
	input->update_mouse_position(100072, 99928, 720);
	CHECK(std::fabs(input->get_axis(ice::input::mouse_x) - .1f) < .0001f);
	CHECK(std::fabs(input->get_axis(ice::input::mouse_y) + .1f) < .0001f);
	input->show_curs();
	input->update_mouse_position(100, 100, 720);
	CHECK(input->get_axis(ice::input::mouse_x) == 0);
	TestState                first, second;
	ice::util::state_machine machine;
	machine.push_state(&first);
	machine.push_state(&second);
	CHECK(machine.pop_state() == &second && second.pops == 1 && machine.peek_state() == &first);
	CHECK(machine.pop_state() == &first && first.pops == 1 && machine.peek_state() == nullptr);
	std::puts("Core runtime checks passed");
	return 0;
}
