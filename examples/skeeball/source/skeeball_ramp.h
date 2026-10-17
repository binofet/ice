#ifndef __SKEE_BALL_RAMP_H__
#define __SKEE_BALL_RAMP_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/audio/sound.h"
#include "ice/render/model.h"

#include "ice/math/vec2.h"
namespace ice::assets {
class content_loader;
}
class btDiscreteDynamicsWorld;
class btTriangleIndexVertexArray;
class btCollisionShape;
class btCollisionObject;
class btMotionState;

// class b2Body;

/*! Provides a puck object
 **/
class Ramp {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	Ramp(void);
	~Ramp(void);

	ice::result init(ice::assets::content_loader *pContent, btDiscreteDynamicsWorld *pWorld);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime);
	ice::result render(void);

private:
	ice::result init_physics(btDiscreteDynamicsWorld *pWorld);

	ice::math::mat4 trans_; //! puck transformation matrix

	ice::assets::content_loader *content_;

	ice::render::model *table_; //! Table Object

	btDiscreteDynamicsWorld *pworld_;

	btTriangleIndexVertexArray *index_vertex_arrays_;
	btCollisionShape           *col_shape_;

	btCollisionObject *col_object_;
	btCollisionObject *col_object2_;
	btCollisionObject *col_object3_;

	void *vb_;

	ice::math::vec3 *verts_;
	int             *ib_;
};

#endif //__SKEE_BALL_RAMP_H__