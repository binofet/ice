#pragma once

#include "ice/render/texture.h"
namespace ice::render {

class material {
public:
	material(void);
	virtual ~material(void);

	virtual ice::result StartPass(void);
	virtual ice::result EndPass(void);

protected:
	texture *m_pTex0;
	texture *m_pTex1;
	texture *m_pTex2;
	texture *m_pTex3;

	ice::uint m_ptrEffect;

	char m_szTechnique[32];
};

} // namespace ice::render
