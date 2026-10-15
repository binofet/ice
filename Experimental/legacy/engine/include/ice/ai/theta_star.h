#pragma once
namespace ice::ai {
#if 0

#ifndef THETA_STAR_H
#define THETA_STAR_H
} // namespace ice::ai

#include "ice/ai/theta_star_common.h"
namespace ice::ai {


/*! This class implements ThetaStar for the Pathing interface
**/
class ThetaStar : public Pathing
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    ThetaStar(void);
    virtual ~ThetaStar(void){Cleanup();};

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

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE FUNCTIONS
    void UpdateVertex(FRINGE_ELEMENT S, FRINGE_ELEMENT Sprime);

    bool LOS(FRINGE_ELEMENT S, FRINGE_ELEMENT SP);

    ICPATHNODE Successor(int index, ice::uchar direction, int* sind);

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    int               m_numFringe;
    int               m_numClosed;
    FRINGE_ELEMENT    m_Fringe[MAX_SEARCH];
    FRINGE_ELEMENT    m_Closed[MAX_SEARCH];
};

#endif // THETA_STAR_H

#endif
} // namespace ice::ai
