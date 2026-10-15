#ifndef __PUCK_H__
#define __PUCK_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "gameplay.h"
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
class Puck {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	Puck(void);
	~Puck(void);

	ice::result init(ice::assets::content_loader *pContent, b2World *pWorld);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime);
	ice::result render(void);

	void reset(void);

	b2Body *get_phys(void) { return phys_; };

private:
	ice::result init_physics(b2World *pWorld);

	ice::math::mat4 trans_; //! puck transformation matrix

	gameplay::RailRecovery rail_recovery_;
	float                  timer_;

	ice::assets::content_loader *content_;

	ice::render::model  *puck_;
	ice::audio::sound_i *sound_; //! Sound of puck hitting the boundries
	b2Body              *phys_;
};

#endif //__PUCK_H__