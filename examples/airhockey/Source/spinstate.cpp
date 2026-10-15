//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "spinstate.h"

#include "ice/input/input.h"

#include "ice/math/rand.h"

extern ice::math::vec2 bl;
extern ice::math::vec2 tl;
extern ice::math::vec2 tr;
extern ice::math::vec2 br;

SpinState::SpinState() {
	table_      = NULL;
	table2_     = NULL;
	table3_     = NULL;
	table4_     = NULL;
	content_    = NULL;
	ground_tex_ = NULL;
	ground_     = NULL;

	halfx    = 500.0;
	halfy    = 500.0;
	numrow   = 50;
	numcol   = 50;
	edgeDist = (halfx * 2) / (numrow - 1);

	(cam_pos)  = ice::math::vec3(0.0f, 0.750f, -1.54f);
	(look_pos) = ice::math::vec3(0.0f, 0.0f, 0.0f);

	render_top_only_ = false;
	update_top_only_ = false;
}

SpinState::~SpinState() {}

/*! Initializes the games state
 *
 *  @param          pContentLoader      Pointer to main content loader
 *  @returns        ICRESULT            Success/failure/warnings of init
 **/
ice::result SpinState::init(ice::assets::content_loader *pContentLoader) {
	if (!pContentLoader)
		return IC_FAIL_GEN;

	content_ = pContentLoader;

	ice::render::graphics_device *pDev = content_->get_device();

	// CREATE A TEMPORARY GROUND OBJECT

	ice::render::vert_def vert_def;
	vert_def.num_prims = (numrow - 1) * (numcol - 1) * 2;
	vert_def.vert_size = sizeof(ice::render::vrt_dif);
	vert_def.num_verts = (numrow - 1) * (numcol - 1) * 4;
	vert_def.usage     = IC_VERT_USAGE_STATIC;
	vert_def.prim_type = ice::render::prim_tlist;
	vert_def.vert_type = ice::render::ic_vert_dif;
	pDev->create_vb(&ground_, &vert_def);

	ice::render::vert_lock vertLock;
	ground_->lock(&vertLock);

	ice::render::vrt_dif *vb = (ice::render::vrt_dif *)vertLock.data;

	ice::uint cur_vert = 0;

	for (int row = 0; row < numrow - 1; ++row) {
		for (int col = 0; col < numcol - 1; ++col) {
			ice::math::real x1 = (col * edgeDist) - halfx;
			ice::math::real y1 = (row * edgeDist) - halfy;
			ice::math::real x2 = ((col + 1) * edgeDist) - halfx;
			ice::math::real y2 = ((row + 1) * edgeDist) - halfy;

			(vb[cur_vert].pos)  = ice::math::vec3(x1, -10.0f, y1);
			(vb[cur_vert++].uv) = ice::math::vec2(0.0f, 0.0f);

			(vb[cur_vert].pos)  = ice::math::vec3(x1, -10.0f, y2);
			(vb[cur_vert++].uv) = ice::math::vec2(0.0f, 1.0f);

			(vb[cur_vert].pos)  = ice::math::vec3(x2, -10.0f, y2);
			(vb[cur_vert++].uv) = ice::math::vec2(1.0f, 1.0f);

			(vb[cur_vert].pos)  = ice::math::vec3(x2, -10.0f, y1);
			(vb[cur_vert++].uv) = ice::math::vec2(1.0f, 0.0f);
		}
	}

	ground_->unlock();
	content_->load("Resource/textures/fire.tga", &ground_tex_);

	ice::render::index_def ind_def;
	ind_def.index_size = ice::render::ic_index_size_16;
	ind_def.num_index  = (numrow - 1) * (numcol - 1) * 6;
	ind_def.usage      = IC_INDEX_USAGE_STATIC;
	pDev->create_ib(&ground_ib_, &ind_def);

	ice::render::index_lock iblock;
	ground_ib_->lock(&iblock);

	short    *ib        = (short *)iblock.data;
	ice::uint cur_index = 0;

	for (int row = 0; row < numrow - 1; ++row) {
		for (int col = 0; col < numcol - 1; ++col) {
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 1;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 2;

			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 2;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4 + 3;
			ib[cur_index++] = (numcol - 1) * row * 4 + col * 4;
		}
	}
	ground_ib_->unlock();

	ice::result res = IC_OK;

	// load the table model
	res |= content_->load("Resource/models/table.icm", &table_);
	res |= content_->load("Resource/models/table_2.icm", &table2_);
	// res |= m_pContent->Load("Resource/models/table_3.icm",&m_pTable3);
	// res |= m_pContent->Load("Resource/models/table_4.icm",&m_pTable4);

	(transform_) = ice::math::mat4(1);

	// get us a camera
	camera_.get_projection() = glm::perspectiveLH_ZO(
	    float((40.0f * IC_PI / 180.0f)), float(1280.0f / 720.0f), float(1.0f), float(10000000.0f));

	org_len = glm::length(cam_pos);
	camera_.get_view_mat() =
	    ice::math::camera_pose_lh(cam_pos, look_pos, ice::math::vec3(0.0f, 1.0f, 0.0f));

	return res;
} // END FUNCTION Init(icContentLoader* pContentLoader)

/*! Updates the current game that is in session
 *
 *  @param          fDeltaTime          Elapsed time since last update
 *  @param          bFinished           Pointer, set to true if ste is done
 *  @returns        ICRESULT            Success/failure/warnings of update
 **/
ice::result SpinState::update(const float fDeltaTime, bool *bFinished) {
	ice::input::input *pInput = ice::input::input::instance();
	short              x, y;
	pInput->get_pos(&x, &y);
	ice::math::vec2 curPos((ice::math::real)x, (ice::math::real)y);

	// camera movement
	ice::math::mat4 &view = camera_.get_view_mat();

	static float x_rot = 0.0f;
	static float y_rot = 0.0f;

	ice::math::vec3 new_pos = cam_pos;

	x_rot += 0.2f * fDeltaTime;

	while (x_rot >= 2 * IC_PI)
		x_rot -= 2 * IC_PI;

	ice::math::mat4 temp;
	(temp)        = ice::math::mat4(1);
	(temp)        = glm::rotate(temp, x_rot, ice::math::vec3(0, 1, 0));
	new_pos       = ice::math::transform_direction(temp, ice::math::vec3(new_pos));
	float cur_len = glm::length(cam_pos);
	camera_.get_view_mat() =
	    ice::math::camera_pose_lh(new_pos, look_pos, ice::math::vec3(0.0f, 1.0f, 0.0f));

	ice::render::vert_lock vlock;
	ground_->lock(&vlock);

	ice::render::vrt_dif *vb = (ice::render::vrt_dif *)vlock.data;

	static float curdir = 0.0f;

	ice::math::real u1 = ice::math::cos(curdir) * 0.01f * fDeltaTime;
	ice::math::real v2 = ice::math::sin(curdir) * 0.01f * fDeltaTime;

	ice::math::real t = ice::math::rand_f(0.0001f, 0.001f);
	curdir += ice::math::rand_f(0.0f, 10000000.0f) > 500000.0f ? t : -t;

	ice::uint cur_vert = 0;
	for (int row = 0; row < numrow - 1; ++row) {
		for (int col = 0; col < numcol - 1; ++col) {
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
			vb[cur_vert].uv.x += u1;
			vb[cur_vert++].uv.y += v2;
		}
	}

	ground_->unlock();

	// TODO: if score, win state and such
	return IC_OK;
} // END FUNCTION Update(const float fDeltaTime, bool *bFinished)

/*! Renders the current game that is in session
 *
 *  @returns        ICRESULT        Success/failure/warnings of render
 **/
ice::result SpinState::render(void) {
	ice::render::graphics_device *pDev = content_->get_device();

	// set projection and view matrix
	pDev->set_projection(camera_.get_projection());
	pDev->set_view_matrix(camera_.get_view_mat());

	// pDev->SetCullMode(IC_CULL_OFF);
	//  draw the temporary ground
	(transform_) = ice::math::mat4(1);
	pDev->set_world_transform(transform_);
	pDev->set_texture(0, ground_tex_);
	pDev->draw_indexed_vert_buf(ground_, ground_ib_);

	// draw the table
	(transform_) = glm::rotate(transform_, IC_PI, ice::math::vec3(0, 1, 0));
	(transform_) = glm::rotate(transform_, -IC_PI / 2.0f, ice::math::vec3(1, 0, 0));
	table_->render(transform_);
	table2_->render(transform_);
	// m_pTable3->Render(m_Transform);
	// m_pTable4->Render(m_Transform);
	(transform_) = ice::math::mat4(1);

	return IC_OK;
} // END FUNCTION Render(void)

/*! This gets called when the SpinState gets pushed on the stack
 *  and should reset anything that needs to be reset for each game
 **/
void SpinState::on_push(void) {} // END FUNCTION OnPush(void)

/*! This gets called when the SpinState gets popped off the stack
 *  we might not need it
 **/
void SpinState::on_pop(void) {} // END FUNCTION OnPop(void)