#ifndef __TABLE_H__
#define __TABLE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/audio/sound.h"
#include "ice/render/model.h"

#include "ice/math/vec2.h"
namespace ice::assets {
class content_loader;
}
class b2World;
class b2Body;

/*! Provides a puck object
 **/
class Table {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	Table(void);
	~Table(void);

	ice::result init(ice::assets::content_loader *pContent, b2World *pWorld);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime);
	ice::result render(void);

private:
	ice::result init_physics(b2World *pWorld);

	ice::math::mat4 trans_; //! puck transformation matrix

	ice::assets::content_loader *content_;

	ice::render::model *table_; //! Table Object
	ice::render::model *table2_;
	ice::render::model *table3_;
	ice::render::model *table4_;

	b2Body *wall1_;
	b2Body *wall2_;
	b2Body *wall3_;
	b2Body *wall4_;
	b2Body *wall5_;
	b2Body *wall6_;
	b2Body *wall7_;
	b2Body *wall8_;
};

#endif //__TABLE_H__