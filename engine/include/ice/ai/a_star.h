#pragma once

#include "ice/ai/a_star_common.h"
#include "ice/ai/pathing_system_i.h"
namespace ice::ai {

/*! This class implements Astar for the Pathing interface
 **/
class a_star : public pathing_system_i {
public:
	a_star(void);
	/// Destroy this instance under the ownership and native-context lifetime rules documented by
	/// its type.
	virtual ~a_star(void) { cleanup(); };

	virtual ice::result cleanup(void);

	virtual ice::result create(ice::uint x, ice::uint y, pathnode state);
	virtual ice::result load(const char *file);
	virtual ice::result save(const char *file);

	virtual ice::result render(void);

	virtual bool try_plot_route(const ice::math::vec3 &start_position,
	                            const ice::math::vec3 &end_position, route_buffer &route_buffer);

	/// Reserved navigation invalidation hook; the current A-star implementation returns false
	/// without changing its grid.
	virtual bool try_invalidate_region(const ice::math::vec3 &current_position,
	                                   const ice::math::real &current_radius,
	                                   const ice::math::vec3 &next_position,
	                                   const ice::math::real &next_radius);

	/// Reserved navigation invalidation hook; the current A-star implementation returns false
	/// without changing its grid.
	virtual bool try_invalidate_region(const ice::math::aab_box &current_aab_box,
	                                   const ice::math::aab_box &next_aab_box);

	virtual void fix_neighbors(int x, int y);

	virtual void get_coord(int index, int *x, int *y);

	virtual bool get_closed(int index, int *ph, int *pg);

	/// Borrow a grid cell; caller must supply validated in-range grid coordinates.
	virtual pathnode &get_node(int x, int y) { return data_[y * cols_ + x]; };

	/// Copy node metadata at a valid grid index to caller storage; return false when the index is
	/// outside the grid.
	bool try_get_node(const ice::math::vec3 &world_position, pathnode *node);

	/// Convert a world position into a grid index; false indicates a position outside the
	/// navigation grid.
	bool try_get_node_index(const ice::math::vec3 &world_position, ice::uint *node_index);

	/// Copy the world-space position of a grid node; return false for an invalid index.
	bool try_get_world_position(ice::uint node_index, ice::math::vec3 &world_position);

private:
	void update_vertex(fringe_element s, fringe_element sprime);

	pathnode successor(int index, ice::uchar direction, int *sind);

	ice::math::vec3 world_min_;
	ice::math::vec3 world_max_;
	int             num_fringe_;
	int             num_closed_;
	fringe_element  fringe_[MAX_SEARCH];
	fringe_element  closed_[MAX_SEARCH];

	int rows_;
	int cols_;

	int       num_nodes_;
	pathnode *data_; //! Pathing data

	/// RenderObject    m_Quads;
};

FORCE_INLINE bool a_star::try_get_node(const ice::math::vec3 &world_position, pathnode *node) {
	ice::uint index;
	if (!node || !try_get_node_index(world_position, &index))
		return false;
	*node = data_[index];
	return true;
}

FORCE_INLINE bool a_star::try_get_node_index(const ice::math::vec3 &world_position,
                                             ice::uint             *node_index) {
	if (!node_index || !data_ ||
	    !(world_position.x >= 0 && world_position.y >= 0 && world_position.x < cols_ &&
	      world_position.y < rows_))
		return false;
	*node_index = (ice::uint)world_position.y * cols_ + (ice::uint)world_position.x;
	return true;
}

FORCE_INLINE bool a_star::try_get_world_position(ice::uint        node_index,
                                                 ice::math::vec3 &world_position) {
	if (!data_ || node_index >= (ice::uint)num_nodes_)
		return false;
	(world_position) = ice::math::vec3((ice::math::real)(node_index % cols_),
	                                   (ice::math::real)(node_index / cols_), 0);
	return true;
}

FORCE_INLINE bool
a_star::try_invalidate_region(const ice::math::vec3 &current_position [[maybe_unused]],
                              const ice::math::real &current_radius [[maybe_unused]],
                              const ice::math::vec3 &next_position [[maybe_unused]],
                              const ice::math::real &next_radius [[maybe_unused]]) {
	return false;
}

FORCE_INLINE bool a_star::try_invalidate_region(const ice::math::aab_box &current_aab_box
                                                [[maybe_unused]],
                                                const ice::math::aab_box &next_aab_box
                                                [[maybe_unused]]) {
	return false;
}

} // namespace ice::ai
