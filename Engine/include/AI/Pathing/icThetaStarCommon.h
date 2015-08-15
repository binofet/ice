#if 0

#ifndef PATHING_H
#define PATHING_H


#include "Util/icTime.h"

#define ASTR_W       0x01
#define ASTR_NW      0x02
#define ASTR_N       0x04
#define ASTR_NE      0x08
#define ASTR_E       0x10
#define ASTR_SE      0x20
#define ASTR_S       0x40
#define ASTR_SW      0x80

#define GLXSIZE (1024)
#define GLYSIZE (512)

#define XSIZE   (1024.0f)
#define YSIZE   (512.0f)

#define MAX_SEARCH (8192)


#define INTDIST_OPT       //! USE int distances (scaled x 10000)
#define GIVEN_METHOD        //! USE HEURISTICS FROM THE ASSIGNMENT


//! This doesn't seem to help at all!
//#define BREAK_TIES_W_G      //! (F1 == F2) ? return G1 <?> G2 : F1 <?> F2;


#define ROOT_2X10000   (14142)//1.4142135623730950488016887242097
#define ROOT_2         (1.4142135623730950488016887242097f)

struct FRINGE_ELEMENT
{
    const bool operator < (const FRINGE_ELEMENT& Rhs )const
    {
#ifdef BREAK_TIES_W_G
#ifdef INDIST_OPT
        int f = H + G;
        int rhs_f = Rhs.H + Rhs.G;
        if (f == rhs_f)
            return G < Rhs.G;
        return f < rhs_f;
#else
        float f = H + G;
        float rhs_f = Rhs.H + Rhs.G;
        if (f == rhs_f)
            return G < Rhs.G;
        return f < rhs_f;
#endif
#else
        return H + G < Rhs.H + Rhs.G;
#endif
    };

    const bool operator > (const FRINGE_ELEMENT& Rhs )const
    {
#ifdef BREAK_TIES_W_G
#ifdef INDIST_OPT
        int f = H + G;
        int rhs_f = Rhs.H + Rhs.G;
        if (f == rhs_f)
            return G > Rhs.G;
        return f > rhs_f;
#else
        float f = H + G;
        float rhs_f = Rhs.H + Rhs.G;
        if (f == rhs_f)
            return G > Rhs.G;
        return f > rhs_f;
#endif
#else
        return H + G > Rhs.H + Rhs.G;
#endif
    };

    // this is for the find function
    const bool operator == (const FRINGE_ELEMENT& Rhs) const
    {
        return (index == Rhs.index);
    };

    int parent;
    int index;
#ifdef INTDIST_OPT
    int G;
    int H;
#else
    float G;
    float H;
#endif
};

struct CLOSED_ELEMENT
{
    const bool operator < (const CLOSED_ELEMENT& Rhs )const
    {
        return index < Rhs.index;
    };

    const bool operator > (const CLOSED_ELEMENT& Rhs )const
    {
        return index > Rhs.index;
    };

    const bool operator == (const CLOSED_ELEMENT& Rhs) const
    {
        return (index == Rhs.index);
    };

    int index;
    int parent;
};

/*! Provides an interface for pathing systems
**/
class Pathing
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    Pathing() {m_bOwnsData = true;};
    virtual inline ~Pathing(void){Cleanup();};

    virtual inline ICRESULT Cleanup(void) {return IC_OK;};

    virtual ICRESULT Create(uint x, uint y, ICPATHNODE state) = 0;
    virtual ICRESULT CreateRand(uint x, uint y) = 0;
    virtual ICRESULT Load(const char* szFile) = 0;
    virtual ICRESULT Save(const char* szFile) = 0;

    virtual ICRESULT Render(void) = 0;

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual int PlotRoute(int startx, int starty, int goalx, int goaly,
                          int* routeBuffer, int bufsize, icReal* calcTime) = 0;

    virtual void FixNeighbors(int x, int y) = 0;

    virtual bool GetClosed(int index, int* ph, int* pg) = 0;

    virtual void GetCoord(int index, int* x, int* y) = 0;

    virtual ICPATHNODE& GetNode(int x, int y)
    {
        return m_Data[y * m_cols + x];
    };

    virtual ICPATHNODE* GetData(void) {return m_Data;};

    int& Rows() {return m_rows;};
    int& Cols() {return m_cols;};
    int& NumNode() {return m_NumNodes;};

    void SetData(ICPATHNODE* pData)
    {
        if (m_Data)
            delete[] m_Data;
        m_Data = pData;

        for (int y=0; y<m_rows; ++y)
            for (int x=0; x<m_cols; ++x)
                FixNeighbors(x,y);

        CheckData();
    };

    void SetDataOwner(bool bOwner) {m_bOwnsData = bOwner;};

protected:
    virtual void CheckData(){};

    int             m_rows;
    int             m_cols;

    int             m_NumNodes;
    ICPATHNODE*     m_Data;     //! Pathing data

    RenderObject    m_Quads;

    icTimer         m_Timer;    //! profiling won't hurt
    bool            m_bOwnsData;
};

#endif // PATHING_H

#endif
