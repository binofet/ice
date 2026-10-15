#pragma once
namespace ice::ai {
#if 0

#ifndef __VIS_ASTAR_H__
#define __VIS_ASTAR_H__
} // namespace ice::ai

#include "ice/ai/a_star_common.h"
namespace ice::ai {

struct VisNode
{
    VisNode() { path_index=0; pHead=0; pNext=0;};

    int         path_index;
    VisNode*    pHead;
    VisNode*    pNext;
};

#if 0
struct FRINGE_VIS
{
    FRINGE_VIS() {index=0;parent=0;};
    const bool operator < (const FRINGE_VIS& Rhs )const
    {
        return H + G < Rhs.H + Rhs.G;
    };

    const bool operator > (const FRINGE_VIS& Rhs )const
    {
        return H + G > Rhs.H + Rhs.G;
    };

    const bool operator == (const FRINGE_VIS& Rhs) const
    {
        if (index && Rhs.index)
            return (index->path_index == Rhs.index->path_index);

        return false;
    };

    VisNode* index;
    VisNode* parent;

    int G;
    int H;
};


/*! This class implements Astar for the Pathing interface
**/
class VisAstar : public Pathing
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    VisAstar(void);
    virtual ~VisAstar(void){Cleanup();};

    virtual ice::result Cleanup(void);

    virtual ice::result Create(ice::uint x, ice::uint y, ICPATHNODE state);
    virtual ice::result CreateRand(ice::uint x, ice::uint y);
    virtual ice::result Load(const char* szFile);
    virtual ice::result Save(const char* szFile);

    virtual ice::result Render(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual int PlotRoute(int startx, int starty, int goalx, int goaly,
                          int* routeBuffer, int bufsize, ice::math::real* calcTime);


    virtual void FixNeighbors(int x, int y);

    virtual void GetCoord(int index, int* x, int* y);

    virtual bool GetClosed(int index, int* ph, int* pg);

protected:
    virtual void CheckData();

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE FUNCTIONS
    void UpdateVertex(FRINGE_VIS S, FRINGE_VIS Sprime);

    bool LOS(FRINGE_VIS S, FRINGE_VIS SP);

    ICPATHNODE Successor(int index, ice::uchar direction, int* sind);

    void BuildVis();
    void FixVis(int x, int y);


    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    int               m_numFringe;
    int               m_numClosed;
    FRINGE_VIS        m_Fringe[MAX_SEARCH];
    FRINGE_VIS        m_Closed[MAX_SEARCH];

    VisNode**         m_Data2;
};
#endif // #if 0
#endif //__VIS_ASTAR_H__

#endif
} // namespace ice::ai
