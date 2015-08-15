#if 0

#ifndef THETA_STAR_H
#define THETA_STAR_H


#include "AI/Pathing/icThetaStarCommon.h"


/*! This class implements ThetaStar for the Pathing interface
**/
class ThetaStar : public Pathing
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    ThetaStar(void);
    virtual ~ThetaStar(void){Cleanup();};

    virtual ICRESULT Cleanup(void);

    virtual ICRESULT Create(uint x, uint y, ICPATHNODE state);
    virtual ICRESULT CreateRand(uint x, uint y);
    virtual ICRESULT Load(const char* szFile);
    virtual ICRESULT Save(const char* szFile);

    virtual ICRESULT Render(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual int PlotRoute(int startx, int starty, int goalx, int goaly,
                          int* routeBuffer, int bufsize, icReal* calcTime);


    virtual void FixNeighbors(int x, int y);

    virtual void GetCoord(int index, int* x, int* y);

    virtual bool GetClosed(int index, int* ph, int* pg);

private:
    //////////////////////////////////////////////////////////////////////////
    // PRIVATE FUNCTIONS
    void UpdateVertex(FRINGE_ELEMENT S, FRINGE_ELEMENT Sprime);

    bool LOS(FRINGE_ELEMENT S, FRINGE_ELEMENT SP);

    ICPATHNODE Successor(int index, uchar direction, int* sind);

    //////////////////////////////////////////////////////////////////////////
    // PRIVATE MEMBERS
    int               m_numFringe;
    int               m_numClosed;
    FRINGE_ELEMENT    m_Fringe[MAX_SEARCH];
    FRINGE_ELEMENT    m_Closed[MAX_SEARCH];
};

#endif // THETA_STAR_H

#endif