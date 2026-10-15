
#include "ice/render/render_object.h"

#include "ice/render/texture.h"
namespace ice::render {

/*! c'tor
 **/
render_object::render_object(graphics_device *device) {
	dev_ = device;
	vb_  = NULL;
	ib_  = NULL;
	tex_ = NULL;

	(trans_) = ice::math::mat4(1);
}

/*! d'tor
 **/
render_object::~render_object(void) { cleanup(); }

/*! Releases allocated vertex buffers and such, overloadable
 *
 *  @returns        ICRESULT        Success/failure/warnings of cleanup action
 **/
ice::result render_object::cleanup(void) {
	ice::result result_code = IC_OK;
	if (dev_) {
		// release vertex buffer (if exists)
		if (vb_) {
			result_code |= dev_->release_vb(vb_);
			vb_ = NULL;
		}

		// release index buffer (if exists)
		if (ib_) {
			result_code |= dev_->release_ib(ib_);
			ib_ = NULL;
		}
	}
	return result_code;
}

/*! Creates a renderable sphere
 *
 *  @param          radius          Radius of the sphere
 *  @param          color             Color of the sphere
 *  @param          subdiv          Subdivisions
 **/
ice::result render_object::create_sphere(const ice::math::real  &radius,
                                         const ice::math::color &color, int subdiv) {
	// make sure we are starting fresh
	cleanup();

	// 4 is the minimum number of subdivisions
	subdiv = ice::math::max(4, subdiv);

	// there are two rings, the horizontal rings have half as many sub divisions
	// as the vertical rings.  note there are 2 extra verts for the top and bottom
	vert_def  description;
	ice::uint num_verts   = (subdiv / 2 - 1) * subdiv + 2;
	ice::uint num_prims   = 2 * (subdiv / 2 - 2) * subdiv + 2 * subdiv;
	description.num_prims = num_prims;
	description.vert_size = sizeof(vrtnrmcol);
	description.num_verts = num_verts;
	description.usage     = IC_VERT_USAGE_STATIC;
	description.prim_type = prim_tlist;
	description.vert_type = ic_vert_nrmcol;

	vert_lock vert_lock;

	// create and lock the vertex buffer
	if (ICEFAIL(dev_->create_vb(&vb_, &description)) || ICEFAIL(vb_->lock(&vert_lock))) {
		cleanup();
		return IC_FAIL_GEN;
	}

	// get a pointer to the locked verts
	vrtnrmcol *vb = (vrtnrmcol *)vert_lock.data;

	ice::uint cur_vert = 0;

	// top vert
	(vb[cur_vert].pos)    = ice::math::vec3(0.0f, radius, 0.0f);
	vb[cur_vert].color    = color;
	(vb[cur_vert++].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

	int vertical_subdiv = subdiv / 2;

	if (subdiv < 6)
		vertical_subdiv = 2;

	// create the middle verticies
	ice::math::real alpha, beta, x, y, z, r;
	for (int j = 1; j < vertical_subdiv; ++j) {
		for (int i = 0; i < subdiv; ++i) {
			alpha = (ice::math::real)(2 * i * IC_PI) / (ice::math::real)subdiv;

			beta = ((ice::math::real)j / (ice::math::real)vertical_subdiv) * IC_PI;

			if (subdiv > 5) {
				r = radius * ice::math::sin(beta);
				y = radius * ice::math::cos(beta);
			} else {
				y = 0.0f;
				r = radius;
			}

			x = r * ice::math::cos(alpha);
			z = r * ice::math::sin(alpha);

			(vb[cur_vert].pos) = ice::math::vec3(x, y, z);
			vb[cur_vert].color = color;

			// calc normal
			ice::math::vec3 normal = vb[cur_vert].pos;
			(normal)               = glm::normalize(normal);

			vb[cur_vert++].norm = normal;
		}
	}

	// bottom vert
	(vb[cur_vert].pos)    = ice::math::vec3(0.0f, -radius, 0.0f);
	vb[cur_vert].color    = color;
	(vb[cur_vert++].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

	// todo: check this result
	vb_->unlock();

	index_def ind_def;
	ind_def.index_size = ic_index_size_16;
	ind_def.num_index  = num_prims * 3;
	ind_def.usage      = IC_INDEX_USAGE_STATIC;

	index_lock iblock;

	// make sure we can create and lock the index buffer
	if (ICEFAIL(dev_->create_ib(&ib_, &ind_def)) || ICEFAIL(ib_->lock(&iblock))) {
		cleanup();
		return IC_FAIL_GEN;
	}

	// get pointer to locked indicies
	short    *ib        = (short *)iblock.data;
	ice::uint cur_index = 0;

	// do top ring of faces
	for (int i = 1; i < subdiv + 1; ++i) {
		ib[cur_index++] = (short)i;
		ib[cur_index++] = 0;
		if (i < subdiv)
			ib[cur_index++] = (short)(i + 1);
		else
			ib[cur_index++] = 1;
	}

	// do middle ring faces (there is no middle ring for subdiv == 4 or 5)
	if (subdiv > 5) {
		for (int j = 0; j < vertical_subdiv - 2; ++j) {
			for (int i = 1; i < subdiv + 1; ++i) {
				ib[cur_index++] = (short)((subdiv * j) + i);
				if (i < subdiv)
					ib[cur_index++] = (short)((subdiv * j) + i + 1);
				else
					ib[cur_index++] = (short)((subdiv * j) + 1);
				ib[cur_index++] = (short)((subdiv * (j + 1)) + i);

				ib[cur_index++] = (short)((subdiv * (j + 1)) + i);
				if (i < subdiv) {
					ib[cur_index++] = (short)((subdiv * j) + i + 1);
					ib[cur_index++] = (short)((subdiv * (j + 1)) + i + 1);
				} else {
					ib[cur_index++] = (short)((subdiv * j) + 1);
					ib[cur_index++] = (short)((subdiv * (j + 1)) + 1);
				}
			}
		}
	}

	// do bottom ring faces
	for (int i = -1; i < subdiv - 1; ++i) {
		ib[cur_index++] = (short)(num_verts - subdiv + i);
		ib[cur_index++] = (short)(num_verts - 1);
		if (i < subdiv - 2)
			ib[cur_index++] = (short)(num_verts - subdiv + i + 1);
		else
			ib[cur_index++] = (short)(num_verts - subdiv - 1);
	}

	// todo: check this result
	ib_->unlock();

	return IC_OK;
}
//                           int subdiv)

/// Create a box with the supplied dimensions and color, borrowing its optional texture.
/// Subdivision is currently unsupported; subdiv is retained but ignored.
ice::result render_object::create_box(const ice::math::vec3  &dimensions,
                                      const ice::math::color &color, int subdiv, texture *tex) {
	(void)subdiv; // (warning removal) TODO: allow the cube to be subdivided
	// make sure we are starting fresh
	cleanup();

	vert_def description;
	description.num_prims = 12;
	if (tex) {
		description.vert_size = sizeof(vrtnrm_dif);
		description.vert_type = ic_vert_nrm_dif;
		tex_                  = tex;
	} else {
		description.vert_size = sizeof(vrtnrmcol);
		description.vert_type = ic_vert_nrmcol;
		tex_                  = NULL;
	}
	description.num_verts = 24;
	description.usage     = IC_VERT_USAGE_STATIC;
	description.prim_type = prim_tlist;

	dev_->create_vb(&vb_, &description);

	vert_lock vert_lock;
	vb_->lock(&vert_lock);

	vrtnrmcol  *vb     = (vrtnrmcol *)vert_lock.data;
	vrtnrm_dif *vb_dif = (vrtnrm_dif *)vb;

	ice::math::real halfx = dimensions.x / ice::math::real(2);
	ice::math::real halfy = dimensions.y / ice::math::real(2);
	ice::math::real halfz = dimensions.z / ice::math::real(2);

	// Manually set vertices
	// Note: assume z extending toward you is positive, away negative
	// Face order: x, -x, y, -y, z, -z

	// Begin x face

	(vb[0].pos)  = ice::math::vec3(halfx, -halfy, halfz);
	(vb[0].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	(vb[1].pos)  = ice::math::vec3(halfx, halfy, halfz);
	(vb[1].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	(vb[2].pos)  = ice::math::vec3(halfx, halfy, -halfz);
	(vb[2].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	(vb[3].pos)  = ice::math::vec3(halfx, -halfy, -halfz);
	(vb[3].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	if (tex) {
		(vb_dif[0].uv) = ice::math::vec2(1.0f, 0.0f);
		(vb_dif[1].uv) = ice::math::vec2(1.0f, 1.0f);
		(vb_dif[2].uv) = ice::math::vec2(0.0f, 1.0f);
		(vb_dif[3].uv) = ice::math::vec2(0.0f, 0.0f);
	} else {
		vb[0].color = color;
		vb[1].color = color;
		vb[2].color = color;
		vb[3].color = color;
	}

	// End x face

	// Begin -x face

	(vb[4].pos)  = ice::math::vec3(-halfx, -halfy, halfz);
	(vb[4].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	(vb[5].pos)  = ice::math::vec3(-halfx, halfy, halfz);
	(vb[5].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	(vb[6].pos)  = ice::math::vec3(-halfx, halfy, -halfz);
	(vb[6].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	(vb[7].pos)  = ice::math::vec3(-halfx, -halfy, -halfz);
	(vb[7].norm) = ice::math::vec3(1.0f, 0.0f, 0.0f);

	if (tex) {
		(vb_dif[4].uv) = ice::math::vec2(0.0f, 0.0f);
		(vb_dif[5].uv) = ice::math::vec2(0.0f, 1.0f);
		(vb_dif[6].uv) = ice::math::vec2(1.0f, 1.0f);
		(vb_dif[7].uv) = ice::math::vec2(1.0f, 0.0f);
	} else {
		vb[4].color = color;
		vb[5].color = color;
		vb[6].color = color;
		vb[7].color = color;
	}

	// End -x face

	// Begin y face

	(vb[8].pos)  = ice::math::vec3(-halfx, halfy, halfz);
	(vb[8].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

	(vb[9].pos)  = ice::math::vec3(-halfx, halfy, -halfz);
	(vb[9].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

	(vb[10].pos)  = ice::math::vec3(halfx, halfy, -halfz);
	(vb[10].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

	(vb[11].pos)  = ice::math::vec3(halfx, halfy, halfz);
	(vb[11].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

	if (tex) {
		(vb_dif[8].uv)  = ice::math::vec2(0.0f, 1.0f);
		(vb_dif[9].uv)  = ice::math::vec2(0.0f, 0.0f);
		(vb_dif[10].uv) = ice::math::vec2(1.0f, 0.0f);
		(vb_dif[11].uv) = ice::math::vec2(1.0f, 1.0f);
	} else {
		vb[8].color  = color;
		vb[9].color  = color;
		vb[10].color = color;
		vb[11].color = color;
	}
	// End y face

	// Begin -y face

	(vb[12].pos)  = ice::math::vec3(-halfx, -halfy, halfz);
	(vb[12].norm) = ice::math::vec3(0.0f, -1.0f, 0.0f);

	(vb[13].pos)  = ice::math::vec3(-halfx, -halfy, -halfz);
	(vb[13].norm) = ice::math::vec3(0.0f, -1.0f, 0.0f);

	(vb[14].pos)  = ice::math::vec3(halfx, -halfy, -halfz);
	(vb[14].norm) = ice::math::vec3(0.0f, -1.0f, 0.0f);

	(vb[15].pos)  = ice::math::vec3(halfx, -halfy, halfz);
	(vb[15].norm) = ice::math::vec3(0.0f, -1.0f, 0.0f);

	if (tex) {
		(vb_dif[12].uv) = ice::math::vec2(0.0f, 0.0f);
		(vb_dif[13].uv) = ice::math::vec2(0.0f, 1.0f);
		(vb_dif[14].uv) = ice::math::vec2(1.0f, 1.0f);
		(vb_dif[15].uv) = ice::math::vec2(1.0f, 0.0f);
	} else {
		vb[12].color = color;
		vb[13].color = color;
		vb[14].color = color;
		vb[15].color = color;
	}
	// End -y face

	// Begin z face

	(vb[16].pos)  = ice::math::vec3(-halfx, -halfy, halfz);
	(vb[16].norm) = ice::math::vec3(0.0f, 0.0f, 1.0f);

	(vb[17].pos)  = ice::math::vec3(-halfx, halfy, halfz);
	(vb[17].norm) = ice::math::vec3(0.0f, 0.0f, 1.0f);

	(vb[18].pos)  = ice::math::vec3(halfx, halfy, halfz);
	(vb[18].norm) = ice::math::vec3(0.0f, 0.0f, 1.0f);

	(vb[19].pos)  = ice::math::vec3(halfx, -halfy, halfz);
	(vb[19].norm) = ice::math::vec3(0.0f, 0.0f, 1.0f);

	if (tex) {
		(vb_dif[16].uv) = ice::math::vec2(1.0f, 0.0f);
		(vb_dif[17].uv) = ice::math::vec2(1.0f, 1.0f);
		(vb_dif[18].uv) = ice::math::vec2(0.0f, 1.0f);
		(vb_dif[19].uv) = ice::math::vec2(0.0f, 0.0f);
	} else {
		vb[16].color = color;
		vb[17].color = color;
		vb[18].color = color;
		vb[19].color = color;
	}
	// End z face

	// Begin -z face

	(vb[20].pos)  = ice::math::vec3(-halfx, -halfy, -halfz);
	(vb[20].norm) = ice::math::vec3(0.0f, 0.0f, -1.0f);

	(vb[21].pos)  = ice::math::vec3(-halfx, halfy, -halfz);
	(vb[21].norm) = ice::math::vec3(0.0f, 0.0f, -1.0f);

	(vb[22].pos)  = ice::math::vec3(halfx, halfy, -halfz);
	(vb[22].norm) = ice::math::vec3(0.0f, 0.0f, -1.0f);

	(vb[23].pos)  = ice::math::vec3(halfx, -halfy, -halfz);
	(vb[23].norm) = ice::math::vec3(0.0f, 0.0f, -1.0f);

	if (tex) {
		(vb_dif[20].uv) = ice::math::vec2(0.0f, 0.0f);
		(vb_dif[21].uv) = ice::math::vec2(0.0f, 1.0f);
		(vb_dif[22].uv) = ice::math::vec2(1.0f, 1.0f);
		(vb_dif[23].uv) = ice::math::vec2(1.0f, 0.0f);
	} else {
		vb[20].color = color;
		vb[21].color = color;
		vb[22].color = color;
		vb[23].color = color;
	}
	// End -z face

	vb_->unlock();

	index_def ind_def;
	ind_def.index_size = ic_index_size_16;
	ind_def.num_index  = 6 * 2 * 3;
	ind_def.usage      = IC_INDEX_USAGE_STATIC;
	dev_->create_ib(&ib_, &ind_def);

	index_lock iblock;
	ib_->lock(&iblock);

	short    *ib        = (short *)iblock.data;
	ice::uint cur_index = 0;

	// Create index buffer for each face (4 vertices per face)
	for (ice::uint vert_index = 0; vert_index < description.num_verts; vert_index += 4) {
		// First triangle on face
		ib[cur_index++] = (short)vert_index;
		ib[cur_index++] = (short)(vert_index + 1);
		ib[cur_index++] = (short)(vert_index + 2);
		// Second triangle on face
		ib[cur_index++] = (short)vert_index;
		ib[cur_index++] = (short)(vert_index + 2);
		ib[cur_index++] = (short)(vert_index + 3);
	}

	ib_->unlock();

	return IC_OK;
}
//                        int subdiv)

/// Create a plane with the supplied dimensions and row/column vertex counts.
/// The legacy implementation currently ignores color and uses its authored checker pattern.
ice::result render_object::create_plane(const ice::math::real &length, const ice::math::real &width,
                                        const ice::math::color &color [[maybe_unused]], int numrow,
                                        int numcol) {
	// make sure we are starting fresh
	cleanup();

	vert_def description;
	description.num_prims = (numrow - 1) * (numcol - 1) * 2;
	description.vert_size = sizeof(vrtnrmcol);
	description.num_verts = (numrow - 1) * (numcol - 1) * 4;
	description.usage     = IC_VERT_USAGE_STATIC;
	description.prim_type = prim_tlist;
	description.vert_type = ic_vert_nrmcol;
	dev_->create_vb(&vb_, &description);

	vert_lock vert_lock;
	vb_->lock(&vert_lock);

	vrtnrmcol *vb = (vrtnrmcol *)vert_lock.data;

	ice::uint       cur_vert   = 0;
	ice::math::real halfx      = width / ice::math::real(2);
	ice::math::real halfy      = length / ice::math::real(2);
	ice::math::real edge_distx = width / ice::math::real(numrow - 1);
	ice::math::real edge_disty = length / ice::math::real(numcol - 1);

	// TODO: this currently has extra verts to allow for tiled texturing
	//       need to add the texture option and make this better
	for (int row = 0; row < numrow - 1; ++row) {
		for (int color = 0; color < numcol - 1; ++color) {
			ice::math::real x1 = (color * edge_distx) - halfx;
			ice::math::real y1 = (row * edge_disty) - halfy;
			ice::math::real x2 = ((color + 1) * edge_distx) - halfx;
			ice::math::real y2 = ((row + 1) * edge_disty) - halfy;

			(vb[cur_vert].pos)    = ice::math::vec3(x1, 0.0f, y1);
			vb[cur_vert].color    = color;
			(vb[cur_vert++].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

			(vb[cur_vert].pos)    = ice::math::vec3(x1, 0.0f, y2);
			vb[cur_vert].color    = color;
			(vb[cur_vert++].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

			(vb[cur_vert].pos)    = ice::math::vec3(x2, 0.0f, y2);
			vb[cur_vert].color    = color;
			(vb[cur_vert++].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);

			(vb[cur_vert].pos)    = ice::math::vec3(x2, 0.0f, y1);
			vb[cur_vert].color    = color;
			(vb[cur_vert++].norm) = ice::math::vec3(0.0f, 1.0f, 0.0f);
		}
	}

	vb_->unlock();

	index_def ind_def;
	ind_def.index_size = ic_index_size_16;
	ind_def.num_index  = (numrow - 1) * (numcol - 1) * 6;
	ind_def.usage      = IC_INDEX_USAGE_STATIC;
	dev_->create_ib(&ib_, &ind_def);

	index_lock iblock;
	ib_->lock(&iblock);

	short    *ib        = (short *)iblock.data;
	ice::uint cur_index = 0;

	for (int row = 0; row < numrow - 1; ++row) {
		for (int color = 0; color < numcol - 1; ++color) {
			ib[cur_index++] = (short)((numcol - 1) * row * 4 + color * 4);
			ib[cur_index++] = (short)((numcol - 1) * row * 4 + color * 4 + 1);
			ib[cur_index++] = (short)((numcol - 1) * row * 4 + color * 4 + 2);

			ib[cur_index++] = (short)((numcol - 1) * row * 4 + color * 4 + 2);
			ib[cur_index++] = (short)((numcol - 1) * row * 4 + color * 4 + 3);
			ib[cur_index++] = (short)((numcol - 1) * row * 4 + color * 4);
		}
	}
	ib_->unlock();
	return IC_OK;
}
//                          int subdiv)

/// Copy num_points positions into a line-list or line-strip buffer with the supplied color.
/// The input array is borrowed for this call; this object owns the uploaded buffers.
ice::result render_object::create_lines(ice::math::vec3 *verts, const ice::math::color &color,
                                        int num_points, bool list) {
	// make sure we are starting fresh
	cleanup();

	if (dev_ && num_points > 1) {
		vert_def description;

		// line strip or list?
		if (!list) {
			description.num_prims = num_points - 1;
			description.prim_type = prim_lstrip;
		} else {
			description.num_prims = num_points / 2;
			description.prim_type = prim_llist;
		}
		description.num_verts = num_points;
		description.vert_size = sizeof(vrtcol);
		description.usage     = IC_VERT_USAGE_DYNAMIC;
		description.vert_type = ic_vert_col;

		// create vertex buffer on the rendering device
		if (ICEOK(dev_->create_vb(&vb_, &description))) {
			vert_lock vert_lock;

			// lock the buffer to fill it with our data
			if (ICEOK(vb_->lock(&vert_lock))) {
				// get pointer to locked data
				vrtcol *pvb = (vrtcol *)vert_lock.data;

				// fill verts
				for (int i = 0; i < num_points; ++i) {
					pvb[i].pos   = verts[i];
					pvb[i].color = color;
				}

				// unlock data, should check this result too
				vb_->unlock();

				return IC_OK;
			}
			return IC_FAIL_LOCK_VB;
		}
		return IC_FAIL_CREATE_VB;
	}
	return IC_FAIL_NO_DEVICE;
}
//	                         int numPoints, bool list)

/*! Overloadable update function
 *
 *  @param          delta_seconds      Elapsed time in seconds since last update
 *  @returns        ICRESULT        Success/failure/warnings of render call
 **/
ice::result render_object::update(const float delta_seconds) {
	(void)delta_seconds; // warning removal
	return IC_OK;
}

/*! Overloadable render function
 *
 *  @returns        ICRESULT        Success/failure/warnings of render call
 **/
ice::result render_object::render(void) {
	if (tex_)
		dev_->set_texture(0, tex_);

	dev_->set_world_transform(trans_);
	if (ib_)
		return dev_->draw_indexed_vert_buf(vb_, ib_);

	return dev_->draw_vert_buf(vb_);
}

} // namespace ice::render
