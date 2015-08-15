#ifndef __IC_A_STAR_H__
#define __IC_A_STAR_H__

#include "AI/Pathing/icAStarCommon.h"
#include "AI/Pathing/icPathingSystemI.h"


/*! This class implements Astar for the Pathing interface
**/
class icAStar : public icPathingSystemI
{
public:
	//////////////////////////////////////////////////////////////////////////
	// LIFECYCLE
	icAStar(void);
	virtual ~icAStar(void){Cleanup();};

	virtual ICRESULT Cleanup(void);

	virtual ICRESULT Create(uint x, uint y, ICPATHNODE state);
	virtual ICRESULT Load(const char* szFile);
	virtual ICRESULT Save(const char* szFile);

	virtual ICRESULT Render(void);

	//////////////////////////////////////////////////////////////////////////
	// OPERATIONS
	virtual bool TryPlotRoute(
		const icVector3& startPosition,
		const icVector3& endPosition,
		icRouteBuffer& routeBuffer);

	virtual bool TryInvalidateRegion(
		const icVector3& currentPosition,
		const icReal& currentRadius,
		const icVector3& nextPosition,
		const icReal& nextRadius);

	virtual bool TryInvalidateRegion(
		const icAABBox& currentAABBox,
		const icAABBox& nextAABBox);


	virtual void FixNeighbors(int x, int y);

	virtual void GetCoord(int index, int* x, int* y);

	virtual bool GetClosed(int index, int* ph, int* pg);

	virtual ICPATHNODE& GetNode(int x, int y)
	{
		return m_Data[y * m_cols + x];
	};

	bool TryGetNode(const icVector3& worldPosition, ICPATHNODE* node);

	bool TryGetNodeIndex(const icVector3& worldPosition, uint* nodeIndex);

	bool TryGetWorldPosition(uint nodeIndex, icVector3& worldPosition);

private:
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE FUNCTIONS
	void UpdateVertex(FRINGE_ELEMENT S, FRINGE_ELEMENT Sprime);

	ICPATHNODE Successor(int index, uchar direction, int* sind);


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE MEMBERS
	icVector3         _worldMin;
	icVector3         _worldMax;
	int               m_numFringe;
	int               m_numClosed;
	FRINGE_ELEMENT    m_Fringe[MAX_SEARCH];
	FRINGE_ELEMENT    m_Closed[MAX_SEARCH];


	int             m_rows;
	int             m_cols;

	int             m_NumNodes;
	ICPATHNODE*     m_Data;     //! Pathing data

	//RenderObject    m_Quads;
};


FORCE_INLINE bool icAStar::TryGetNode(const icVector3& worldPosition, ICPATHNODE* node)
{
	int x = (int)worldPosition.x;
	int y = (int)worldPosition.y;

	int index = y*m_cols + x;
	if (index < m_NumNodes)
	{
		node = &m_Data[index];
		return true;
	}
	return false;
}

FORCE_INLINE bool icAStar::TryGetNodeIndex(const icVector3& worldPosition, uint* nodeIndex)
{
	int x = (int)worldPosition.x;
	int y = (int)worldPosition.y;

	int index = y*m_cols + x;
	if (index < m_NumNodes)
	{
		*nodeIndex = index;
		return true;
	}
	return false;
}

FORCE_INLINE bool icAStar::TryGetWorldPosition(uint nodeIndex, icVector3& worldPosition)
{
	// TODO, make this right
	uint x = nodeIndex % m_cols;
	uint y = nodeIndex % m_rows;
	worldPosition.Set((icReal)x, (icReal)y, 0.0f);
	return true;
}

FORCE_INLINE bool icAStar::TryInvalidateRegion(
	const icVector3& currentPosition,
	const icReal& currentRadius,
	const icVector3& nextPosition,
	const icReal& nextRadius)
{
	return false;
}

FORCE_INLINE bool icAStar::TryInvalidateRegion(
	const icAABBox& currentAABBox,
	const icAABBox& nextAABBox)
{
	return false;
}

#endif // __IC_A_STAR_H__
