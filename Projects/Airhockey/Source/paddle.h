#ifndef __PADDLE_H__
#define __PADDLE_H__

//////////////////////////////////////////////////////////////////////////////
// ICE INCLUDES
#include "ice/math/vec2.h"
#include "ice/render/model.h"

//////////////////////////////////////////////////////////////////////////////
// LOCAL INCLUDES
#include "InputHandler.h"

//////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
namespace ice::assets {
class content_loader;
}
class b2World;
class b2Body;

/*! Provides a Paddle object
 **/
class Paddle {
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	Paddle(void);
	~Paddle(void);

	ice::result init(ice::assets::content_loader *pContent, b2World *pWorld);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	ice::result update(const float fDeltaTime);
	ice::result render(void);

	void reset(void);

	void set_input_handler(InputHandler *pHandler) { handler_ = pHandler; };

	InputHandler *get_input_handler(void) { return handler_; };

	b2Body *get_phys(void) { return phys_[0]; };

	int player;

private:
	ice::result init_physics(b2World *pWorld);

	ice::math::mat4 trans_; //! Model transform matrix

	ice::assets::content_loader *content_;

	ice::render::model *model_[3]; //! puck model
	b2Body             *phys_[3];  //! Phyics 0-disc,1-triangle,2-square

	InputHandler *handler_;
};

#endif //__PADDLE_H__