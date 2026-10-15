#pragma once

#include "ice/globals.h"
#include "ice/math/aab_box.h"
#include "ice/math/vec3.h"
#include "ice/util/time.h"
#include <string>
namespace ice::ai {

/// Legacy route-node metadata carried alongside a waypoint.
class path_node_info {
public:
	/// Human-readable identifier; the enclosing object owns the string.
	std::string name;
	/// Legacy node metadata value text.
	std::string vale;
};

/// Caller-owned fixed-capacity route output. Only entries below size are meaningful; node-info
/// pointers are borrowed.
struct route_buffer {
	/// static int MaxPathLength = 1024;

	// int _indicies[1024];
	/// Caller-owned waypoint storage in world coordinates.
	ice::math::vec3 way_points[1024];
	/// Borrowed metadata pointers corresponding to route waypoints.
	const path_node_info *info[1024];
	/// Number of valid entries/bytes as specified by the owning type.
	int size;
};

/*! Provides an interface for pathing systems
 **/
class pathing_system_i {
public:
	/// Render using the current owning graphics context; inspect the result before continuing a
	/// failed frame.
	virtual ice::result render(void) = 0;

	/// Write a route into caller-owned waypoint storage; false indicates no route or insufficient
	/// output capacity.
	virtual bool try_plot_route(const ice::math::vec3 &start_position,
	                            const ice::math::vec3 &end_position,
	                            route_buffer          &route_buffer) = 0;

	/// Reserved navigation invalidation hook; the current A-star implementation returns false
	/// without changing its grid.
	virtual bool try_invalidate_region(const ice::math::vec3 &current_position,
	                                   const ice::math::real &current_radius,
	                                   const ice::math::vec3 &next_position,
	                                   const ice::math::real &next_radius) = 0;

	/// Reserved navigation invalidation hook; the current A-star implementation returns false
	/// without changing its grid.
	virtual bool try_invalidate_region(const ice::math::aab_box &current_aab_box,
	                                   const ice::math::aab_box &next_aab_box) = 0;
};

} // namespace ice::ai
