#include "stdio.h"
#include <math.h>
#include <memory.h>

#include "ice/ai/a_star.h"
#include "ice/math/rand.h"
#include "ice/memory/min_heap.h"
namespace ice::ai {

#define min(x, y) (x < y ? x : y)
#define max(x, y) (x > y ? x : y)

/*! c'tor
 **/
a_star::a_star(void) {
	data_      = NULL;
	num_nodes_ = 0;
	rows_      = 0;
	cols_      = 0;

	num_fringe_ = 0;
	num_closed_ = 0;
}

/*! Cleans up all memories allocated
 *
 *  @returns        ICRESULT        Success/failure of cleanup
 **/
ice::result a_star::cleanup(void) {
	if (data_) {
		delete[] data_;
	}

	data_      = NULL;
	num_nodes_ = 0;
	rows_      = 0;
	cols_      = 0;

	num_fringe_ = 0;
	num_closed_ = 0;
	return IC_OK;
}

/*! Creates a pathing buffer to work on in real time
 *
 *  @param          x           Number of nodes in x
 *  @param          y           Number of nodes in y
 *  @returns        ICRESULT    Success/failure of pathing creation
 **/
ice::result a_star::create(unsigned int x, unsigned int y, pathnode state) {
	ice::result result_code = cleanup();
	if (ICEFAIL(result_code))
		return result_code;
	if (!x || !y || (unsigned long long)x * y > MAX_SEARCH)
		return IC_FAIL_GEN;

	rows_      = y;
	cols_      = x;
	num_nodes_ = x * y;

	data_ = new pathnode[num_nodes_];

	memset(data_, state, sizeof(pathnode) * num_nodes_);

	if (!data_)
		return IC_FAIL_GEN;

	// TODO: DEBUG PATHING RENDERING
	// m_Quads.Create(VERT_TYPE_2D_FLAT_TEX,4*m_cols*m_rows,PRIM_TYPE_QUADS);

	// Texture* tex = Texture::LoadTGA("resource/links.tga");
	// if (!tex)
	//     return IC_FAIL_GEN;

	// m_Quads.SetTexture(tex,NULL);

	// VERT_FLAT_TEX_2D* pvb = (VERT_FLAT_TEX_2D*)m_Quads.Lock();

	// if (!pvb)
	//     return IC_FAIL_GEN;

	// icReal rx = XSIZE/(icReal)m_cols;
	// icReal ry = YSIZE/(icReal)m_rows;

	// int ind=0;
	for (int j = 0; j < rows_; ++j) {
		for (int i = 0; i < cols_; ++i) {
			pathnode mask = 0xFF;
			if (i == 0)
				mask &= (~ASTR_W & ~ASTR_NW & ~ASTR_SW);
			if (j == 0)
				mask &= (~ASTR_S & ~ASTR_SW & ~ASTR_SE);
			if (i == cols_ - 1)
				mask &= (~ASTR_E & ~ASTR_NE & ~ASTR_SE);
			if (j == rows_ - 1)
				mask &= (~ASTR_N & ~ASTR_NW & ~ASTR_NE);

			pathnode &temp = data_[j * cols_ + i];

			temp &= mask;

			// float xmin = (float)(i)*rx;
			// float xmax = (float)(i+1)*rx;
			// float ymin = (float)(j)*ry;
			// float ymax = (float)(j+1)*ry;

			// float u0 = (float)(temp%16)/16.0f;
			// float u1 = u0 + 0.98f/16.0f;
			// float v0 = (float)(temp/16)/16.0f;
			// float v1 = v0 + 0.99f/16.0f;

			// pvb[ind].uv.Set(u0,v0);
			// pvb[ind++].pos.Set(xmin,ymin);

			// pvb[ind].uv.Set(u0,v1);
			// pvb[ind++].pos.Set(xmin,ymax);

			// pvb[ind].uv.Set(u1,v1);
			// pvb[ind++].pos.Set(xmax,ymax);

			// pvb[ind].uv.Set(u1,v0);
			// pvb[ind++].pos.Set(xmax,ymin);
		}
	}

	// m_Quads.Unlock();

	return IC_OK;
}

/*! Loads the pathing data from file
 *
 *  @param          file          File to load
 *  @returns        ICRESULT        Success/failure of pathing load
 **/
ice::result a_star::load(const char *file) {
	FILE *fp = fopen(file, "rb");
	if (!fp)
		return IC_FAIL_GEN;
	ice::uint x = 0, y = 0;
	bool      valid = fread(&x, sizeof(x), 1, fp) == 1 && fread(&y, sizeof(y), 1, fp) == 1;
	fseek(fp, 0, SEEK_END);
	long length = ftell(fp);
	valid       = valid && x && y && x <= 4096 && y <= 4096 && length >= 8 &&
	        (unsigned long long)x * y * sizeof(pathnode) == (unsigned long long)(length - 8);
	if (!valid) {
		fclose(fp);
		return IC_FAIL_GEN;
	}
	ice::result result = create(x, y, 0);
	if (ICEFAIL(result)) {
		fclose(fp);
		return result;
	}
	(world_min_) = ice::math::vec3(0, 0, 0);
	(world_max_) = ice::math::vec3((ice::math::real)x, (ice::math::real)y, 0);
	fseek(fp, 8, SEEK_SET);
	valid = fread(data_, sizeof(pathnode), num_nodes_, fp) == (size_t)num_nodes_;
	fclose(fp);
	if (!valid) {
		cleanup();
		return IC_FAIL_GEN;
	}
	for (int i = 0; i < num_nodes_; ++i)
		fix_neighbors(i % cols_, i / cols_);
	return IC_OK;
}

/// Write grid dimensions and node data to a native file; return a failure status on I/O errors.
ice::result a_star::save(const char *file) {
	FILE *fp = NULL;

#if defined(_MSC_VER) && (_MSC_VER > 1400)
	fopen_s(&fp, szFile, "wb");
#else
	fp = fopen(file, "wb");
#endif

	if (!fp)
		return IC_FAIL_GEN;

	// get the size of the data down
	fwrite(&cols_, sizeof(ice::uint), 1, fp);
	fwrite(&rows_, sizeof(ice::uint), 1, fp);

	// write the data
	fwrite(data_, sizeof(pathnode), num_nodes_, fp);

	fclose(fp);

	return IC_OK;
}

/*! Renders the pathing data
 **/
ice::result a_star::render(void) {
	// m_Quads.Render();

	return IC_OK;
}

/// Find a grid route between world-space endpoints and copy waypoints into route_buffer.
/// False reports invalid endpoints, no path, or insufficient output capacity.
bool a_star::try_plot_route(const ice::math::vec3 &start_position,
                            const ice::math::vec3 &end_position, route_buffer &route_buffer) {
	ice::uchar dir[8] = {0, 1, 2, 3, 4, 5, 6, 7}; //! so that we can randomize the successor order

	ice::uint gi; // = goaly*m_cols + goalx;      //! Goal index
	ice::uint si; // = starty*m_cols + startx;    //! Start index

	if (!try_get_node_index(start_position, &si)) {
		ice::warning("Could not lookup world position");
		return false;
	}

	if (!try_get_node_index(end_position, &gi)) {
		ice::warning("Could not lookup world position");
		return false;
	}

	int startx = si % cols_;
	int starty = si / cols_;

	int goalx = gi % cols_;
	int goaly = gi / cols_;

	num_fringe_ = 0; //! Reset structures used in the search
	num_closed_ = 0;

	int dx = abs(goalx - startx);
	int dy = abs(goaly - starty);

	fringe_element start;
	start.index  = si;
	start.parent = si;
	start.g      = 0;
#ifdef INTDIST_OPT
	start.h = ROOT_2X10000 * min(dx, dy) + 10000 * max(dx, dy) - 10000 * min(dx, dy);
#elif defined(GIVEN_METHOD)
	start.H = ROOT_2 * (float)min(dx, dy) + (float)max(dx, dy) - (float)min(dx, dy);
#else
	start.H = sqrtf((float)(dx * dx + dy * dy));
#endif

	fringe_[0] = start;
	num_fringe_++; //! Initialize fringe with start node

	// while there are nodes to explore...
	while (num_fringe_ > 0) {
		fringe_element s = ice::memory::min_heap_dequeue(fringe_, num_fringe_--);

		// check indexes for goal node
		if (s.index == static_cast<int>(gi)) {
			int       num_nodes = 0;
			ice::uint index     = s.parent;
			while (index != si) {
				if (num_nodes < 1024) {
					// routeBuffer._indicies[num_nodes] = index;
					try_get_world_position(index, route_buffer.way_points[num_nodes]);
					// TODO hook up the other info
					num_nodes++;
				}
				fringe_element temp;
				temp.index = index;
				int i      = ice::memory::min_heap_find(closed_, num_closed_, temp);
				if (i >= 0)
					index = closed_[i].parent;
				else
					ice::errorf("Failed to find node in the heap!");
			}
			route_buffer.size = num_nodes;
			return true;
		}

		// add S to the closed list, we've been here man!
		ice::memory::min_heap_insert(closed_, num_closed_++, s);

		// randomize successor order
		for (int i = 0; i < 8; ++i)
			ice::math::swap(dir[ice::math::rand_i(0, 7)], dir[ice::math::rand_i(0, 7)]);

		// go through the successors of S
		for (int i = 0; i < 8; ++i) {
			ice::uchar mask = (1 << dir[i]);

			fringe_element s_prime;

			// get successor to S
			pathnode node = successor(s.index, mask, &s_prime.index);

			if (!node)
				continue;

			int closed_index = ice::memory::min_heap_find(closed_, num_closed_, s_prime);
			int fringe       = ice::memory::min_heap_find(fringe_, num_fringe_, s_prime);
			if (closed_index < 0) {
				if (fringe < 0) {
#ifdef INTDIST_OPT
					s_prime.g = 0x7FFFFFFF;
#else
					s_prime.G = (float)0x7FFFFFFF;
#endif
					s_prime.parent = -1;
				}

				{
					int spx, spy;
					get_coord(s_prime.index, &spx, &spy);

					int dx = abs(goalx - spx);
					int dy = abs(goaly - spy);

#ifdef INTDIST_OPT
					s_prime.h =
					    ROOT_2X10000 * min(dx, dy) + 10000 * max(dx, dy) - 10000 * min(dx, dy);
#elif defined(GIVEN_METHOD)
					s_prime.H =
					    ROOT_2 * (float)min(dx, dy) + (float)max(dx, dy) - (float)min(dx, dy);
#else
					s_prime.H = sqrtf((float)(dx * dx + dy * dy));
#endif
				}

				update_vertex(s, s_prime);
			}
		}
	}

	return false;
}
//                        int* routeBuffer, int bufsize)

/*! Updates a vertex
 *
 *  @param          s           Vertex
 *  @param          sprime      Successor of S
 **/
void a_star::update_vertex(fringe_element s, fringe_element sprime) {
	int sx, sy, spx, spy;

	get_coord(s.index, &sx, &sy);
	get_coord(sprime.index, &spx, &spy);

	int dx = abs(sx - spx);
	int dy = abs(sy - spy);

#ifdef INTDIST_OPT
	int cost = (dx & dy) ? ROOT_2X10000 : 10000;
#elif defined(GIVEN_METHOD)
	float COST = sqrtf((float)(dx * dx + dy * dy));
#else
	float COST = (dx & dy) ? ROOT_2 : 1.0f;
#endif
	if (s.g + cost < sprime.g) {
		sprime.g = s.g + cost;

		// set parent
		sprime.parent = s.index;

		int index = ice::memory::min_heap_find(fringe_, num_fringe_, sprime);
		if (index >= 0)
			ice::memory::min_heap_decrease_key(fringe_, index, sprime);
		else
			ice::memory::min_heap_insert(fringe_, num_fringe_++, sprime);
	}
}

/*! Get a successor node from index as specified by direction
 *
 *  @param          index           Index the the node to expand
 *  @param          direction       Direction of node to expand
 *  @param[out]     sind            Successor returned index
 *  @ICPATHNODE     ICPATHNODE      Copy of the pathing node
 **/
pathnode a_star::successor(int index, ice::uchar direction, int *sind) {
	int x, y;
	get_coord(index, &x, &y);

	if (direction & ASTR_E) {
		if (x < cols_ - 1) {
			*sind = (y * cols_) + x + 1;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_N) {
		if (y < rows_ - 1) {
			*sind = ((y + 1) * cols_) + x;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_NE) {
		if (y < rows_ - 1 && x < cols_ - 1) {
			*sind = ((y + 1) * cols_) + x + 1;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_NW) {
		if (x > 0 && y < rows_ - 1) {
			*sind = ((y + 1) * cols_) + x - 1;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_W) {
		if (x > 0) {
			*sind = (y * cols_) + x - 1;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_SW) {
		if (x > 0 && y > 0) {
			*sind = ((y - 1) * cols_) + x - 1;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_S) {
		if (y > 0) {
			*sind = ((y - 1) * cols_) + x;
			return data_[*sind];
		}
		return 0;
	}
	if (direction & ASTR_SE) {
		if (x < cols_ - 1 && y > 0) {
			*sind = ((y - 1) * cols_) + x + 1;
			return data_[*sind];
		}
		return 0;
	}
	return 0;
}

/*! Called to adjust the links to neighbors if a node is modified
 *
 *  @param      x           X position of node that was modified
 *  @param      y           Y position of node that was modified
 **/
void a_star::fix_neighbors(int x, int y) {
	int w = x - 1;
	int e = x + 1;
	int n = y + 1;
	int s = y - 1;

	pathnode node = get_node(x, y);

	// VERT_FLAT_TEX_2D* pvb = (VERT_FLAT_TEX_2D*)m_Quads.Lock();

	// int ind = (y * m_cols + x) * 4;

	// float u0 = (float)(node%16)/16.0f;
	// float u1 = u0 + 1.0f/16.0f;
	// float v0 = (float)(node/16)/16.0f;
	// float v1 = v0 + 1.0f/16.0f;

	// pvb[ind+0].uv.Set(u0,v0);
	// pvb[ind+1].uv.Set(u0,v1);
	// pvb[ind+2].uv.Set(u1,v1);
	// pvb[ind+3].uv.Set(u1,v0);

	// w
	if (x > 0) {
		pathnode &temp = get_node(w, y);
		if (node & ASTR_W)
			temp |= ASTR_E;
		else
			temp &= ~ASTR_E;

		// ind = (y * m_cols + w) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// sw
	if (x > 0 && y > 0) {
		pathnode &temp = get_node(w, s);
		// temp &= ((node&ASTR_SW) ? ASTR_NE : ~ASTR_NE);
		if (node & ASTR_SW)
			temp |= ASTR_NE;
		else
			temp &= ~ASTR_NE;

		// ind = (s * m_cols + w) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// nw
	if (x > 0 && y < rows_ - 1) {
		pathnode &temp = get_node(w, n);
		// temp &= ((node&ASTR_NW) ? ASTR_SE : ~ASTR_SE);
		if (node & ASTR_NW)
			temp |= ASTR_SE;
		else
			temp &= ~ASTR_SE;

		// ind = (n * m_cols + w) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// s
	if (y > 0) {
		pathnode &temp = get_node(x, s);
		// temp &= ((node&ASTR_S) ? ASTR_N : ~ASTR_N);
		if (node & ASTR_S)
			temp |= ASTR_N;
		else
			temp &= ~ASTR_N;

		// ind = (s * m_cols + x) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// n
	if (y < rows_ - 1) {
		pathnode &temp = get_node(x, n);
		// temp &= ((node&ASTR_N) ? ASTR_S : ~ASTR_S);
		if (node & ASTR_N)
			temp |= ASTR_S;
		else
			temp &= ~ASTR_S;

		// ind = (n * m_cols + x) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// ne
	if (y < rows_ - 1 && x < cols_ - 1) {
		pathnode &temp = get_node(e, n);
		// temp &= ((node&ASTR_NE) ? ASTR_SW : ~ASTR_SW);
		if (node & ASTR_NE)
			temp |= ASTR_SW;
		else
			temp &= ~ASTR_SW;

		// ind = (n * m_cols + e) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// e
	if (x < cols_ - 1) {
		pathnode &temp = get_node(e, y);
		// temp &= ((node&ASTR_E) ? ASTR_W : ~ASTR_W);
		if (node & ASTR_E)
			temp |= ASTR_W;
		else
			temp &= ~ASTR_W;

		// ind = (y * m_cols + e) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// se
	if (x < cols_ - 1 && y > 0) {
		pathnode &temp = get_node(e, s);
		// temp &= ((node&ASTR_SE) ? ASTR_NW : ~ASTR_NW);
		if (node & ASTR_SE)
			temp |= ASTR_NW;
		else
			temp &= ~ASTR_NW;

		// ind = (s * m_cols + e) * 4;

		// float u0 = (float)(temp%16)/16.0f;
		// float u1 = u0 + 1.0f/16.0f;
		// float v0 = (float)(temp/16)/16.0f;
		// float v1 = v0 + 1.0f/16.0f;

		// pvb[ind+0].uv.Set(u0,v0);
		// pvb[ind+1].uv.Set(u0,v1);
		// pvb[ind+2].uv.Set(u1,v1);
		// pvb[ind+3].uv.Set(u1,v0);
	}

	// m_Quads.Unlock();
}

/*! Gets the x,y coords of a node by its index
 *
 *  @param          index           Index into m_Data
 *  @param          x               Pointer to store x position
 *  @param          y               Pointer to store y position
 **/
void a_star::get_coord(int index, int *x, int *y) {
	*x = index % cols_;
	*y = index / cols_;
}

/*! Asks if the given index exists in the closed list
 *
 *      if it does, returns true and fills in some heuristic information
 *
 *  @param          index           Index to check
 *  @param[out]     ph              Pointer to store H
 *  @param[out]     pg              Pointer to store G
 *  @returns        bool            true on success
 **/
bool a_star::get_closed(int index, int *ph, int *pg) {
	fringe_element temp;
	temp.index = index;
	int i      = ice::memory::min_heap_find(closed_, num_closed_, temp);
	if (i >= 0) {
		fringe_element el = closed_[i];
		*ph               = el.h;
		*pg               = el.g;
		return true;
	}

	return false;
}
} // namespace ice::ai
