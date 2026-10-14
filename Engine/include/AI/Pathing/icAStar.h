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


FORCE_INLINE bool icAStar::TryGetNode(const icVector3& p, ICPATHNODE* node)
{
    uint index;
    if (!node || !TryGetNodeIndex(p, &index)) return false;
    *node = m_Data[index];
    return true;
}

FORCE_INLINE bool icAStar::TryGetNodeIndex(const icVector3& p, uint* index)
{
    if (!index || !m_Data || !(p.x >= 0 && p.y >= 0 && p.x < m_cols && p.y < m_rows)) return false;
    *index = (uint)p.y*m_cols + (uint)p.x;
    return true;
}

FORCE_INLINE bool icAStar::TryGetWorldPosition(uint index, icVector3& p)
{
    if (!m_Data || index >= (uint)m_NumNodes) return false;
    p.Set((icReal)(index % m_cols), (icReal)(index / m_cols), 0);
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
