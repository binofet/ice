#ifndef __IC_PATHING_INTERFACE_H__
#define __IC_PATHING_INTERFACE_H__

#include "Math/Vector/icVector3.h"
#include "Math/icAABBox.h"
#include "Memory/icString.h"
#include "Util/icTime.h"

class icPathNodeInfo
{
public:
	icString _name;
	icString _vale;
};

struct icRouteBuffer
{
	//static int MaxPathLength = 1024;

	//int _indicies[1024];
	icVector3 _wayPoints[1024];
	const icPathNodeInfo* _info[1024];
	int _size;
};

/*! Provides an interface for pathing systems
**/
class icPathingSystemI
{
public:
	virtual ICRESULT Render(void) = 0;

	virtual bool TryPlotRoute(
		const icVector3& startPosition,
		const icVector3& endPosition,
		icRouteBuffer& routeBuffer) = 0;

	virtual bool TryInvalidateRegion(
		const icVector3& currentPosition,
		const icReal& currentRadius,
		const icVector3& nextPosition,
		const icReal& nextRadius) = 0;

	virtual bool TryInvalidateRegion(
		const icAABBox& currentAABBox,
		const icAABBox& nextAABBox) = 0;

};

#endif // __IC_PATHING_INTERFACE_H__
