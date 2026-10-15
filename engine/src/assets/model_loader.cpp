#include "ice/assets/content_loader.h"

#include "ice/io/file.h"

#include "ice/render/index_buf.h"
#include "ice/render/model.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"
namespace ice::assets {

/*! Model loader
 *
 *  @param          file_name      Name of the model to load
 *  @param[in/out]  out_object           Storage place for loaded texture
 *  @returns        ICRESULT        Success/failure of texture load
 **/
template <>
ice::result content_loader::load<ice::render::model>(const char          *file_name,
                                                     ice::render::model **out_object) {
	if (!file_name || !strlen(file_name)) {
		*out_object = NULL;
		return IC_WARN_GEN;
	}

	// try to open the model file
	ice::io::file file;
	if (ICEFAIL(file.open(file_name, ICFMREAD_EXISTING))) {
		ice::warningf("Content loader could not open: %s", file_name);
		return IC_FAIL_GEN;
	}

	ice::render::e_model model_header = {};

	size_t bytes_read = 0;

	// read the ice eader
	if (ICEFAIL(file.read(&model_header, sizeof(ice::render::e_model), &bytes_read))) {
		ice::warningf("Content loader could not read: %s", file_name);
		return IC_FAIL_GEN;
	}

	// Get new model and set its device pointer
	ice::render::model *model = new ice::render::model();
	model->device_            = device_;

	// CREATE VERTEX BUFFER
	ice::render::vert_def vertex_description = {};
	vertex_description.num_verts             = model_header.num_verts;
	switch (model_header.model_version) {
	case ICE_MODEL_FORMAT:
		vertex_description.vert_type = (ice::render::vert)model_header.vert_type;
		switch (vertex_description.vert_type) {
		case ice::render::ic_vert_dif:
			vertex_description.vert_size = sizeof(ice::render::vrt_dif);
			break;
		case ice::render::ic_vert_nrm_dif:
			vertex_description.vert_size = sizeof(ice::render::vrtnrm_dif);
			break;
		default:
			vertex_description.vert_size = 0;
		}
		break;
	case 101:
		switch (model_header.vert_type) {
		case 2:
			vertex_description.vert_type = ice::render::ic_vert_dif;
			vertex_description.vert_size = sizeof(ice::render::vrt_dif);
			break;
		case 8:
			vertex_description.vert_type = ice::render::ic_vert_nrm_dif;
			vertex_description.vert_size = sizeof(ice::render::vrtnrm_dif);
			break;
		default:
			vertex_description.vert_size = 0;
		}
		break;
	case 100:
		vertex_description.vert_type = ice::render::ic_vert_dif;
		vertex_description.vert_size = sizeof(ice::render::vrt_dif);
		file.set_pos(sizeof(ice::render::e_model_100)); // rewind
		break;
	default:
		delete model;
		ice::warningf("Invalid model format: %s", file_name);
		return IC_FAIL_GEN;
	}

	vertex_description.usage     = IC_VERT_USAGE_STATIC;
	vertex_description.num_prims = model_header.num_ind / 3;
	vertex_description.prim_type = ice::render::prim_tlist;

	ice::render::vert_buffer *vb;
	if (ICEFAIL(device_->create_vb(&vb, &vertex_description))) {
		delete model;
		ice::warningf("Content loader could not create models (%s) vertex buffer",
		              model_header.model_name);
		return IC_FAIL_GEN;
	}
	// fill the vertex buffer
	ice::render::vert_lock vert_lock;
	if (ICEFAIL(vb->lock(&vert_lock))) {
		device_->release_vb(vb);
		delete model;
		ice::warningf("Content loader count not lock models (%s) vert buffer",
		              model_header.model_name);
		return IC_FAIL_GEN;
	}
	if (ICEFAIL(
	        file.read(vert_lock.data, vert_lock.num_verts * vert_lock.vert_size, &bytes_read))) {
		device_->release_vb(vb);
		delete model;
		ice::warningf("Content loader count not read models (%s) vert buffer",
		              model_header.model_name);
		return IC_FAIL_GEN;
	}
	vb->unlock();
	model->vb_ = vb;

	// CREATE INDEX BUFFER
	ice::render::index_def index_description = {};
	index_description.num_index              = model_header.num_ind;
	index_description.index_size             = ice::render::ic_index_size_32;
	index_description.usage                  = IC_INDEX_USAGE_STATIC;

	ice::render::index_buf *ib;
	if (ICEFAIL(device_->create_ib(&ib, &index_description))) {
		device_->release_vb(vb);
		delete model;
		ice::warningf("Content loader could not create models (%s) index buffer",
		              model_header.model_name);
		return IC_FAIL_GEN;
	}
	ice::render::index_lock ind_lock;
	if (ICEFAIL(ib->lock(&ind_lock))) {
		device_->release_vb(vb);
		device_->release_ib(ib);
		delete model;
		ice::warningf("Content loader could not lock models (%s) index buffer",
		              model_header.model_name);
		return IC_FAIL_GEN;
	}
	ice::uint indsize = (ind_lock.index_size == ice::render::ic_index_size_16) ? 2 : 4;
	if (ICEFAIL(file.read(ind_lock.data, ind_lock.num_index * indsize, &bytes_read))) {
		device_->release_vb(vb);
		device_->release_ib(ib);
		delete model;
		ice::warningf("Content loader could not read models (%s) index buffer",
		              model_header.model_name);
		return IC_FAIL_GEN;
	}
	ib->unlock();
	model->ib_ = ib;

	// CREATE THE DIFFUSE TEXTURE
	ice::render::texture *tex;
	if (strlen(model_header.dif_tex) >= 5) {
		if (ICEOK(load(model_header.dif_tex, &tex))) {
			model->dif_tex_ = tex;
		}
	}

	// CREATE THE NORMAL TEXTURE
	if (strlen(model_header.norm_tex) >= 5) {
		if (ICEOK(load(model_header.norm_tex, &tex))) {
			model->norm_tex_ = tex;
		}
	}

	// CREATE THE SPECULAR TEXTURE
	if (strlen(model_header.spec_tex) >= 5) {
		if (ICEOK(load(model_header.spec_tex, &tex))) {
			model->spec_tex_ = tex;
		}
	}

	*out_object = model;
	return IC_OK;
}

/*! Model unloader
 *
 *  @param[in/out]  out_object           Loaded model
 *  @returns        ICRESULT        Success/failure of model unload
 **/
template <>
ice::result content_loader::unload<ice::render::model>(ice::render::model **out_object) {
	if (!*out_object)
		return IC_OK;

	if (device_) {
		// TODO: reference counting!
		// if (refcount == 0)
		{
			ice::render::model *model = *out_object;
			device_->release_tex(model->dif_tex_);
			device_->release_tex(model->norm_tex_);
			device_->release_tex(model->spec_tex_);

			device_->release_vb(model->vb_);
			device_->release_ib(model->ib_);
		}

		*out_object = NULL;
		return IC_OK;
	}
	return IC_FAIL_NO_DEVICE;
}

} // namespace ice::assets
