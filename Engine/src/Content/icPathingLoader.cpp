#include "Content/icContentLoader.h"

#include "AI/Pathing/icPathingSystemI.h"
#include "AI/Pathing/icAStar.h"
//#include "AI/Pathing/icThetaStar.h"
//#include "AI/Pathing/icVisibilityAStar.h"
#include "Core/IO/icFile.h"

/*! Audio loader
 *
 *  @param          szFileName      Name of the pathing file to load
 *  @param[in/out]  ppObj           Storage place for pointer to pathing object
 *  @returns        ICRESULT        Success/failure of pathing load
**/
template<>
ICRESULT icContentLoader::Load<icPathingSystemI>(const char* szFileName,
                                         icPathingSystemI** ppObj)
{
	icAStar* astar = new icAStar();
	if (ICEOK(astar->Load(szFileName)))
	{
		*ppObj = astar;
		//m_pAudio->LoadSource(szFileName);
		//m_pAudio->GetSound(szFileName, ppObj);

		return IC_OK;
	}
	delete astar;
	*ppObj = 0;
	return IC_FAIL_GEN;
}// END FUNCTION Load<icPathingSystemI>(const char* szFileName, icPathingSystemI** ppObj)


