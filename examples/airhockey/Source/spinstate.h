#ifndef __SPIN_STATE_H__
#define __SPIN_STATE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/assets/content_loader.h"
#include "ice/render/camera.h"
#include "ice/render/chase_cam.h"
#include "ice/render/index_buf.h"
#include "ice/render/model.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"
#include "ice/util/state_machine.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "paddle.h"
#include "puck.h"

#include "../Box2D/Box2D.h"

class SpinState : public ice::util::state {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	SpinState(void);
	~SpinState(void);

	ice::result init(ice::assets::content_loader *pContentLoader);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime, bool *bFinished);
	ice::result render(void);

	void on_push(void);
	void on_pop(void);

private:
	// cameras
	ice::render::camera camera_;

	ice::assets::content_loader *content_;

	// renderable objects
	ice::render::texture     *ground_tex_; //! Fire ground (temp)
	ice::render::vert_buffer *ground_;
	ice::render::index_buf   *ground_ib_;
	ice::math::real           halfx;
	ice::math::real           halfy;
	int                       numrow;
	int                       numcol;
	float                     edgeDist;

	ice::math::mat4     transform_;
	ice::render::model *table_; //! Table Object
	ice::render::model *table2_;
	ice::render::model *table3_;
	ice::render::model *table4_;

	// camera movement stuff
	ice::math::vec3 cam_pos;
	ice::math::vec3 look_pos;
	float           org_len;
};

#endif //__SPIN_STATE_H__