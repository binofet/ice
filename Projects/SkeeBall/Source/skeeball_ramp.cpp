
#include "skeeball_ramp.h"

#include "ice/assets/content_loader.h"
#include "ice/io/file.h"

#include "ice/render/model.h"

#include "ice/render/vert_buffer.h"

#include "btBulletDynamicsCommon.h"

/*! c'tor
 **/
Ramp::Ramp(void) {
	table_ = NULL;

	verts_ = NULL;
	ib_    = NULL;

	vb_ = NULL;
} // END FUNCTION Ramp(void)

/*! d'tor
 **/
Ramp::~Ramp(void) {
	if (verts_)
		delete[] verts_;

	if (ib_)
		delete[] ib_;

	if (vb_)
		free(vb_);
} // END FUNCTION ~Ramp(void)

/*! Initializes the puck
 *
 *  @param          pContent        Pointer to content loader
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of initialization
 **/
ice::result Ramp::init(ice::assets::content_loader *pContent, btDiscreteDynamicsWorld *pWorld) {
	ice::result res = IC_OK;
	if (!pContent)
		return IC_FAIL_GEN;

	content_ = pContent;

	// load the Ramp model
	res |= content_->load("Resource/models/skeeball_table.icm", &table_);

	(trans_) = ice::math::mat4(1);
	(trans_) = glm::rotate(trans_, -IC_PI / 2.0f, ice::math::vec3(1, 0, 0));

	res |= init_physics(pWorld);

	return res;
} // END FUNCTION Init(icContentLoader* pContent, b2World* pWorld)

/*! Updates the Ramp
 *
 *  @param      fDeltaTime      Elapsed time since last update
 *  @returns    ICRESULT        Success/failure of update
 **/
ice::result Ramp::update(const float fDeltaTime) {
	return IC_OK;
} // END FUNCTION Update(const float fDeltaTime)

// static icModel* model;

/*! Renders the Ramp
 *
 *  @returns        ICRESULT        Success/failure of render
 **/
ice::result Ramp::render(void) {
	ice::result res = IC_OK;

	ice::math::mat4 temp_mat;

	res = content_->get_device()->set_ambient_color(ice::math::color::bred);

	if (pworld_) {
		col_object_->getWorldTransform().getOpenGLMatrix(glm::value_ptr(temp_mat));
		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));
		res |= table_->render(temp_mat);

		col_object2_->getWorldTransform().getOpenGLMatrix(glm::value_ptr(temp_mat));
		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));
		res |= table_->render(temp_mat);

		col_object3_->getWorldTransform().getOpenGLMatrix(glm::value_ptr(temp_mat));
		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));
		res |= table_->render(temp_mat);
	} else {
		(temp_mat) = ice::math::mat4(1);
		(temp_mat) = glm::rotate(temp_mat, -IC_PI_HALF, ice::math::vec3(1, 0, 0));
		ice::math::set_position(temp_mat, ice::math::vec3(-105.422f, 0.0f, 0.0f));
		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));
		table_->render(temp_mat);
		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));

		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));
		ice::math::set_position(temp_mat, ice::math::vec3(0.0f, 0.0f, 0.0f));
		table_->render(temp_mat);
		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));

		(temp_mat) = glm::scale(temp_mat, ice::math::vec3(-1.0f, 1.0f, 1.0f));
		ice::math::set_position(temp_mat, ice::math::vec3(105.422f, 0.0f, 0.0f));
		table_->render(temp_mat);
	}

	return res;
} // END FUNCTION Render(void)

/*! Initializes the physics for the Ramp
 *
 *  @param          pWorld          Pointer to physics world
 *  @returns        ICRESULT        Success/failure of physics initialization
 **/
ice::result Ramp::init_physics(btDiscreteDynamicsWorld *pWorld) {
	if (!pWorld)
		return IC_OK;
	;

	pworld_ = pWorld;

	if (verts_)
		delete[] verts_;

	if (ib_)
		delete[] ib_;

	// m_pContent->Load("Resource/models/skeeball_collision.icm",&model);

	//// try to open the model file
	ice::io::file file;
	if (ICEFAIL(file.open("Resource/models/skeeball_collision.icm", ICFMREAD_EXISTING))) {
		ice::warningf("Content loader could not open: %s",
		              "Resource/models/skeeball_collision.icm");
		return IC_FAIL_GEN;
	}

	ice::render::e_model model_header = {0};

	size_t sizeread = 0;

	// read the ice model header
	if (ICEFAIL(file.read(&model_header, sizeof(ice::render::e_model), &sizeread))) {
		ice::warningf("Content loader could not read: %s",
		              "Resource/models/skeeball_collision.icm");
		return IC_FAIL_GEN;
	}

	verts_ = NULL;

	int totalTriangles = model_header.num_ind / 3;

	int totalVerts = model_header.num_verts;

	verts_ = new ice::math::vec3[model_header.num_verts];

	ib_ = new int[model_header.num_ind];

	vb_ = malloc(sizeof(ice::render::vrtnrm_dif) * model_header.num_verts);

	size_t read;
	// CREATE VERTEX BUFFER
	switch (model_header.model_version) {
	case ICE_MODEL_FORMAT: {
		switch (model_header.vert_type) {
		case ice::render::ic_vert_dif: {
			ice::render::vrt_dif cur_vert;
			for (int i = 0; i < model_header.num_verts; ++i) {
				file.read(&cur_vert, sizeof(ice::render::vrt_dif), &read);
				verts_[i] = cur_vert.pos;
			}
		} break;
		case ice::render::ic_vert_nrm_dif: {
			ice::render::vrtnrm_dif cur_vert;
			for (int i = 0; i < model_header.num_verts; ++i) {
				file.read(&cur_vert, sizeof(ice::render::vrtnrm_dif), &read);
				verts_[i] = cur_vert.pos;
			}
		} break;
		default:;
		}
	} break;
	case 101: {
		switch (model_header.vert_type) {
		case 2: {
			ice::render::vrt_dif cur_vert;
			for (int i = 0; i < model_header.num_verts; ++i) {
				file.read(&cur_vert, sizeof(ice::render::vrt_dif), &read);
				verts_[i] = cur_vert.pos;
			}
		} break;
		case 8: {
			ice::render::vrtnrm_dif cur_vert;
			for (int i = 0; i < model_header.num_verts; ++i) {
				file.read(&cur_vert, sizeof(ice::render::vrt_dif), &read);
				verts_[i] = cur_vert.pos;
			}
		} break;
		default: {
			ice::render::vrtnrm_dif cur_vert;
			for (int i = 0; i < model_header.num_verts; ++i) {
				file.read(&cur_vert, sizeof(ice::render::vrt_dif), &read);
				verts_[i] = cur_vert.pos;
			}
		}
		}
	} break;
	case 100: {
		file.set_pos(sizeof(ice::render::e_model_100)); // rewind

		ice::render::vrt_dif cur_vert;
		for (int i = 0; i < model_header.num_verts; ++i) {
			file.read(&cur_vert, sizeof(ice::render::vrt_dif), &read);
			verts_[i] = cur_vert.pos;
		}
	} break;
	default:
		ice::warningf("Invalid model format: %s", "Resource/models/skeeball_collision.icm");
		return IC_FAIL_GEN;
	}

	file.read(ib_, model_header.num_ind * sizeof(int), &read);

	index_vertex_arrays_ =
	    new btTriangleIndexVertexArray(totalTriangles, ib_, 3 * sizeof(int), model_header.num_verts,
	                                   (btScalar *)verts_, sizeof(ice::math::vec3));

	btVector3 aabbMin(-1000, -1000, -1000), aabbMax(1000, 1000, 1000);
	col_shape_ = new btBvhTriangleMeshShape(index_vertex_arrays_, true, aabbMin, aabbMax);

	col_object_ = new btCollisionObject();
	col_object_->setCollisionShape(col_shape_);

	col_object2_ = new btCollisionObject();
	col_object2_->setCollisionShape(col_shape_);

	col_object3_ = new btCollisionObject();
	col_object3_->setCollisionShape(col_shape_);

	btTransform transform;
	transform.setFromOpenGLMatrix(glm::value_ptr(trans_));
	col_object_->setWorldTransform(transform);

	transform.setOrigin(btVector3(-105.422f, 0.0f, 0.0f));
	col_object2_->setWorldTransform(transform);

	transform.setOrigin(btVector3(105.422f, 0.0f, 0.0f));
	col_object3_->setWorldTransform(transform);

	pWorld->addCollisionObject(col_object_);  //, 0x0001, 0x00FF);
	pWorld->addCollisionObject(col_object2_); //, 0x0001, 0x00FF);
	pWorld->addCollisionObject(col_object3_); //, 0x0001, 0x00FF);

	return IC_OK;
} // END FUNCTION InitPhysics(b2World* pWorld)