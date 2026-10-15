#include "ice/assets/content_loader.h"

#include "ice/ai/a_star.h"
#include "ice/ai/pathing_system_i.h"
namespace ice::assets {
// #include "ice/ai/theta_star.h"
// #include "ice/ai/visibility_a_star.h"
} // namespace ice::assets
#include "ice/io/file.h"
namespace ice::assets {

/*! Audio loader
 *
 *  @param          file_name      Name of the pathing file to load
 *  @param[in/out]  out_object           Storage place for pointer to pathing object
 *  @returns        ICRESULT        Success/failure of pathing load
 **/
template <>
ice::result
content_loader::load<ice::ai::pathing_system_i>(const char                 *file_name,
                                                ice::ai::pathing_system_i **out_object) {
	ice::ai::a_star *astar = new ice::ai::a_star();
	if (ICEOK(astar->load(file_name))) {
		*out_object = astar;
		// m_pAudio->LoadSource(szFileName);
		// m_pAudio->GetSound(szFileName, ppObj);

		return IC_OK;
	}
	delete astar;
	*out_object = 0;
	return IC_FAIL_GEN;
}

} // namespace ice::assets
