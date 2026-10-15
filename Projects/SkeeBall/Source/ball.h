#ifndef __BALL_H__
#define __BALL_H__

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
class btRigidBody;

class Ball {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	Ball(void);
	~Ball(void);

	ice::result init(ice::assets::content_loader *pContent, btDiscreteDynamicsWorld *pWorld,
	                 const ice::math::vec3 &pos);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime);
	ice::result render(void);

	void enable(bool bEnable);

	void set_pos(const ice::math::vec3 &pos);

	void get_mat(ice::math::mat4 *mat);

	btRigidBody *body_;

private:
	ice::result init_physics(btDiscreteDynamicsWorld *pWorld, const ice::math::vec3 &pos);

	ice::assets::content_loader *content_;

	static ice::render::model *m_pBallModel;

	btDiscreteDynamicsWorld *world_;

	static btCollisionShape *m_sphereShape;

	btMotionState *motion_state_;

	bool in_world_;
};

#endif //__BALL_H__