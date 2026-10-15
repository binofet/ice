
#include "ice/render/model.h"

#include "ice/render/index_buf.h"
#include "ice/render/texture.h"
#include "ice/render/vert_buffer.h"

#include "ice/render/graphics_device.h"
namespace ice::render {

model::model(void) {
	vb_       = NULL;
	ib_       = NULL;
	dif_tex_  = NULL;
	norm_tex_ = NULL;
	spec_tex_ = NULL;

	device_ = NULL;
}

model::~model(void) {}

ice::result model::render(const ice::math::mat4 &trans) {
	if (device_) {
		ice::result result_code = IC_OK;

		device_->set_world_transform(trans);

		// m_pDevice->SetCullMode(IC_CULL_ANTICLOCK);

		if (dif_tex_)
			result_code |= device_->set_texture(0, dif_tex_);

		// if (m_normTex)
		//     res |= m_pDevice->SetTexture(1, m_normTex);

		// if (m_specTex)
		//     res |= m_pDevice->SetTexture(2, m_specTex);

		if (ib_ && vb_) {
			result_code |= device_->draw_indexed_vert_buf(vb_, ib_);
		} else if (vb_) {
			result_code |= device_->draw_vert_buf(vb_);
		}

		// m_pDevice->SetCullMode(IC_CULL_CLOCKWISE);

		device_->set_texture(0, NULL);
		return result_code;
	}
	return IC_FAIL_NO_DEVICE;
}

} // namespace ice::render
