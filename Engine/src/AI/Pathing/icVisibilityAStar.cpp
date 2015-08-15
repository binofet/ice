
#include "AI/Pathing/icVisibilityAStar.h"

#if 0
#include "stdio.h"
#include <math.h>

#include "Memory/icMinHeap.h"

#include "Math/icRand.h"


FORCE_INLINE bool Grid(ICPATHNODE node)
{
    return node != 0xFF;
};


/*! c'tor
**/
VisAstar::VisAstar(void)
{
    m_Data = NULL;
    m_Data2 = NULL;
    m_NumNodes = 0;
    m_rows = 0;
    m_cols = 0;

    m_numFringe = 0;
    m_numClosed = 0;
}// END FUNCTION VisAstar(void)


/*! Cleans up all memories allocated
 *
 *  @returns        ICRESULT        Success/failure of cleanup
**/
ICRESULT VisAstar::Cleanup(void)
{
    if (m_Data && m_bOwnsData)
        delete[] m_Data;

    if (m_Data2)   
    {
        for (int i=0; i<m_NumNodes; ++i)
        {
            if (m_Data2[i])
                SAFEDEL(m_Data2[i]);
        }
        free(m_Data2);
        m_Data2 = NULL;
    }

    m_Data = NULL;
    m_NumNodes = 0;
    m_rows = 0;
    m_cols = 0;

    m_numFringe = 0;
    m_numClosed = 0;
    return IC_OK;
}// END FUNCTION Cleanup(void)


/*! Creates a pathing buffer to work on in real time
 *
 *  @param          x           Number of nodes in x
 *  @param          y           Number of nodes in y
 *  @returns        ICRESULT    Success/failure of pathing creation
**/
ICRESULT VisAstar::Create(unsigned int x, unsigned int y, ICPATHNODE state)
{
    ICRESULT res = Cleanup();
    if (ICEFAIL(res))
        return res;

    m_rows = y;
    m_cols = x;
    m_NumNodes= x*y;

    m_Quads.Create(VERT_TYPE_2D_FLAT_TEX,4*m_cols*m_rows,PRIM_TYPE_QUADS);

    Texture* tex = Texture::LoadTGA("resource/links.tga");
    if (!tex)
        return IC_FAIL_GEN;

    m_Data = new ICPATHNODE[m_NumNodes];

    memset(m_Data,state,sizeof(ICPATHNODE)*m_NumNodes);

    if (!m_Data)
        return IC_FAIL_GEN;

    m_Data2 = (VisNode**)malloc(sizeof(VisNode*)*m_NumNodes);

    m_Quads.SetTexture(tex,NULL);

    VERT_FLAT_TEX_2D* pvb = (VERT_FLAT_TEX_2D*)m_Quads.Lock();

    if (!pvb)
        return IC_FAIL_GEN;

    icReal rx = XSIZE/(icReal)m_cols;
    icReal ry = YSIZE/(icReal)m_rows;

    int ind=0;
    for (int j=0; j<m_rows; ++j)
    {
        for (int i=0; i<m_cols; ++i)
        {
            ICPATHNODE mask = 0xFF;
            if (i == 0)
                mask &= (~ASTR_W & ~ASTR_NW & ~ASTR_SW);
            if (j == 0)
                mask &= (~ASTR_S & ~ASTR_SW & ~ASTR_SE);
            if (i == m_cols-1)
                mask &= (~ASTR_E & ~ASTR_NE & ~ASTR_SE);
            if (j == m_rows-1)
                mask &= (~ASTR_N & ~ASTR_NW & ~ASTR_NE);

            ICPATHNODE& temp = m_Data[j * m_cols + i];

            temp &= mask;

            m_Data2[j*m_cols+i] = new VisNode();
            m_Data2[j*m_cols+i]->path_index = j*m_cols+i;


            float xmin = (float)(i)*rx;
            float xmax = (float)(i+1)*rx;
            float ymin = (float)(j)*ry;
            float ymax = (float)(j+1)*ry;

            float u0 = (float)(temp%16)/16.0f;
            float u1 = u0 + 1.0f/16.0f;
            float v0 = (float)(temp/16)/16.0f;
            float v1 = v0 + 1.0f/16.0f;

            pvb[ind].uv.Set(u0,v0);
            pvb[ind++].pos.Set(xmin,ymin);

            pvb[ind].uv.Set(u0,v1);
            pvb[ind++].pos.Set(xmin,ymax);

            pvb[ind].uv.Set(u1,v1);
            pvb[ind++].pos.Set(xmax,ymax);

            pvb[ind].uv.Set(u1,v0);
            pvb[ind++].pos.Set(xmax,ymin);
        }
    }

    m_Quads.Unlock();

    BuildVis();


    return IC_OK;
}// END FUNCTION Create(unsigned int x, unsigned int y)


/*! Creates a pathing buffer with random nodes
 *
 *      Note: (0,0) -> (x,y) is guaranteed to work
 *
 *  @param          x           Number of nodes in x
 *  @param          y           Number of nodes in y
 *  @returns        ICRESULT    Success/failure of pathing creation
**/
ICRESULT VisAstar::CreateRand(uint x, uint y)
{
    ICRESULT res = Create(x,y,0xFF);
    if (ICEFAIL(res))
        return res;



    for (int j = 0; j<m_rows; ++j)
    {
        for (int i = 0; i<m_cols; ++i)
        {
            int e = i+1;
            int w = i-1;
            int s = j-1;

            // 10 % propability of blocked cell
            if (i != 0 && j != 0 && i != m_cols && j != m_rows && 
                icRandF(0.0f,1000.0f) > 900.0f)
            {
                m_Data[j * m_cols + i] = 0;
                FixNeighbors(i,j);
                continue;
            }

            uchar rand_node = (uchar)icRandF(0.0f,255.0f);

            uchar mask = 0xFF;

            if (i == 0)
                mask &= (~ASTR_W & ~ASTR_NW & ~ASTR_SW);
            else
            {
                ICPATHNODE n = GetNode(w,j);
                if (n&ASTR_E)
                    rand_node |= ASTR_W;
                else
                    mask &= ~ASTR_W;

                if (j > 0)
                {
                    n = GetNode(w,s);
                    if (n&ASTR_NE)
                        rand_node |= ASTR_SW;
                    else
                        mask &= ~ASTR_SW;
                }
            }


            if (j == 0)
                mask &= (~ASTR_S & ~ASTR_SW & ~ASTR_SE);
            else
            {
                ICPATHNODE n = GetNode(i,s);
                if (n&ASTR_N)
                    rand_node |= ASTR_S;
                else
                    mask &= ~ASTR_S;

                if (i<m_cols-1)
                {
                    n = GetNode(e,s);
                    if (n&ASTR_NW)
                        rand_node |= ASTR_SE;
                    else
                        mask &= ~ASTR_SE;
                        
                }
            }

            if (i == m_cols-1)
                mask &= (~ASTR_E & ~ASTR_NE & ~ASTR_SE);
            if (j == m_rows-1)
                mask &= (~ASTR_N & ~ASTR_NW & ~ASTR_NE);

            if (i == m_cols-1 && j == m_rows-1)
                rand_node |= (ASTR_W | ASTR_SW | ASTR_S);
            if (i == 0 && j == 0)
                rand_node |= (ASTR_E | ASTR_NE | ASTR_N);

            rand_node = 0xff;

            rand_node &= mask;

            m_Data[j * m_cols + i] = rand_node;
            FixNeighbors(i,j);
        }
    }

    BuildVis();

    return res;
}// END FUNCTION CreateRand(uint x, uint y)


/*! Loads the pathing data from file
 *
 *  @param          szFile          File to load
 *  @returns        ICRESULT        Success/failure of pathing load
**/
ICRESULT VisAstar::Load(const char* szFile)
{
    ICRESULT res = Cleanup();
    if (ICEFAIL(res))
        return res;

    FILE* fp=NULL;

#ifdef _MSC_VER
    fopen_s(&fp, szFile, "rb");
#else
    fp = fopen(szFile, "wb");
#endif

    if (!fp)
        return IC_FAIL_GEN;

    uint x,y;
    fseek(fp,0,SEEK_SET);
    fread(&x,sizeof(uint),1,fp);
    fread(&y,sizeof(uint),1,fp);

    // call create so vert buffers and textures are set up too!
    res = Create(x,y,0x00);

    // read the pathing data in
    fread(m_Data,sizeof(ICPATHNODE),m_NumNodes,fp);

    for (int i=0; i<m_NumNodes; ++i)
    {
        x = i%m_cols;
        y = i/m_cols;
        FixNeighbors(x,y);
    }

    BuildVis();

    fclose(fp);

    return IC_OK;
}// END FUNCTION Load(const char* szFile)


/*! Saves the current pathing data to file
 *
 *  @param          szFile          File to save
 *  @param          ICRESULT        Success/failure of pathing save
**/
ICRESULT VisAstar::Save(const char* szFile)
{
    FILE* fp=NULL;

#ifdef _MSC_VER
    fopen_s(&fp, szFile, "w");
#else
    fp = fopen(szFile, "wb");
#endif

    if (!fp)
        return IC_FAIL_GEN;

    // get the size of the data down
    fwrite(&m_cols,sizeof(uint),1,fp);
    fwrite(&m_rows,sizeof(uint),1,fp);

    // write the data
    fwrite(m_Data,sizeof(ICPATHNODE),m_NumNodes,fp);

    fclose(fp);

    return IC_OK;
}// END FUNCTION Save(const char* szFile)


/*! Renders the pathing data
**/
ICRESULT VisAstar::Render(void)
{
    m_Quads.Render();
    return IC_OK;
}// END FUNCTION Render(void)


/*! Plouts a route from startx,starty to goalx,goaly
 *
 *  @param      startx          Start x pos of the route
 *  @param      starty          Start y pos of the rout
 *  @param      goalx           Goal x pos
 *  @param      goaly           Goal y pos
 *  @param[out] routeBuffer     User provided area for the route buffer
 *  @param      bufsize         Size of users buffer
 *  @param[out] calcTime        Time spent finding solution (in seconds)
 *  @returns    int             Number of nodes in route
**/
int VisAstar::PlotRoute(int startx, int starty, int goalx, int goaly,
                     int* routeBuffer, int bufsize, icReal* calcTime)
{
    return 0;
    m_Timer.Elapsed(); //! Reset timer

    VisNode* GI = m_Data2[goaly*m_rows+goalx];      //! Goal index
    VisNode* SI = m_Data2[starty*m_rows+startx];    //! Start index

    m_numFringe = 0;            //! Reset structures used in the search
    m_numClosed = 0;

    int dx = abs(goalx - startx);
    int dy = abs(goaly - starty);

    FRINGE_VIS start;
    start.index = SI;
    start.parent = SI;
    start.G = 0;
#ifdef INTDIST_OPT
    start.H = ROOT_2X10000 * min(dx,dy) + 10000*max(dx,dy) - 10000*min(dx,dy);
#else
    start.H = sqrtf((float)(dx*dx + dy*dy));
#endif

    m_Fringe[0] = start; m_numFringe++; //! Initialize fringe with start node

    // while there are nodes to explore...
    while (m_numFringe > 0)
    {
        FRINGE_VIS S = icMinHeapDequeue(m_Fringe,m_numFringe--);               

        // check indexes for goal node
        if (S.index == GI)
        {
            int num_nodes = 0;
            VisNode* index = S.parent;
            while (index != SI)
            {
                if (num_nodes < bufsize)
                    routeBuffer[num_nodes++] = S.index->path_index;
                FRINGE_VIS temp;
                temp.index = index;
                int i = icMinHeapFind(m_Closed,m_numClosed,temp);
                if (i >= 0)
                    index = m_Closed[i].parent;
                else
                    assert(0);
            }
            if (calcTime)
                *calcTime = m_Timer.Elapsed();
            return num_nodes;
        }

        // add S to the closed list, we've been here man!
        icMinHeapInsert(m_Closed,m_numClosed++,S);

        // go through the successors of S
        VisNode* cur = S.index->pHead;
        while (cur)
        {
            FRINGE_VIS s_prime;

            s_prime.index = cur;

            int closed_index = icMinHeapFind(m_Closed,m_numClosed,s_prime);
            int fringe = icMinHeapFind(m_Fringe,m_numFringe,s_prime);
            if (closed_index < 0)
            {
                if (fringe < 0)
                {
#ifdef INTDIST_OPT
                    s_prime.G = 0x7FFFFFFF;
#else
                    s_prime.G = (float)0xFFFFFFF;
#endif
                    s_prime.parent = NULL;
                }

                int spx,spy;
                GetCoord(s_prime.index->path_index, &spx,&spy);

                int dx = abs(goalx - spx);
                int dy = abs(goaly - spy);

#ifdef INTDIST_OPT
                s_prime.H = ROOT_2X10000 * min(dx,dy) + 10000*max(dx,dy) - 10000*min(dx,dy);
#else
                s_prime.H = sqrtf((float)(dx*dx + dy*dy));
#endif

                UpdateVertex(S,s_prime);
            }

            cur = cur->pNext;
        }
    }
    if (calcTime)
        *calcTime = m_Timer.Elapsed();
    return 0;
}// END FUNCTION PlotRoute(int startx, int starty, int goalx, int goaly,
 //                        int* routeBuffer, int bufsize)


/*! Updates a vertex
 *
 *  @param          S           Vertex
 *  @param          Sprime      Successor of S
**/
void VisAstar::UpdateVertex(FRINGE_VIS S, FRINGE_VIS Sprime)
{
    int sx,sy,spx,spy;

    GetCoord(S.index->path_index, &sx,&sy);
    GetCoord(Sprime.index->path_index, &spx,&spy);

    int dx = abs(sx - spx);
    int dy = abs(sy - spy);

    
#ifdef INTDIST_OPT
    int COST = ROOT_2X10000 * min(dx,dy) +  10000*max(dx,dy) - 10000*min(dx,dy);
#else
    float COST = sqrtf((float)(dx*dx + dy*dy));
#endif
    if (S.G + COST < Sprime.G)
    {
        Sprime.G = S.G + COST;

        // set parent
        Sprime.parent = S.index;

        int index = icMinHeapFind(m_Fringe,m_numFringe,Sprime);
        if (index >= 0)
            icMinHeapDecreaseKey(m_Fringe,index,Sprime);
        else
            icMinHeapInsert(m_Fringe,m_numFringe++,Sprime);
    }
}// END FUNCTION UpdateVertex(FRINGE_VIS S, FRINGE_VIS Sprime)


/*! Gets line of sight between 2 nodes
 *
 *  @param          S           Start vertex
 *  @param          SP          End vertex
 *  @returns        bool        true if S has LOS to SP
**/
bool VisAstar::LOS(FRINGE_VIS S, FRINGE_VIS SP)
{
    int x0,y0,x1,y1,sx,sy,dx,dy,f;
    
    GetCoord(S.index->path_index,&x0,&y0);
    GetCoord(SP.index->path_index,&x1,&y1);

    f = 0;

    dy = y1 - y0;
    dx = x1 - x0;

    if (dy < 0)
    {
        dy = -dy;
        sy = -1;
    }
    else
        sy = 1;

    if (dx < 0)
    {
        dx = -dx;
        sx = -1;
    }
    else
        sx = 1;

    if (dx >= dy)
    {
        while (x0 != x1)
        {
            int tempx,tempy;
            tempx = x0 + ((sx-1)/2);
            tempy = y0 + ((sy-1)/2);

            f += dy;
            if (f >= dx)
            {
                if (tempx < m_cols && tempx > 0 && tempy < m_rows && tempy > 0)
                    if (Grid(GetNode(tempx,tempy)))
                        return false;

                y0 += sy;
                f -= dx;
            }

            if (tempx < m_cols && tempx > 0 && tempy < m_rows && tempy > 0)
                if (f != 0 && Grid(GetNode(tempx, tempy)))
                    return false;

            if (tempx < m_cols && tempx > 0 && y0 > 0)
                if (dy == 0 && Grid(GetNode(tempx, y0)) && Grid(GetNode(tempx,y0-1)))
                    return false;

            x0 += sx;
        }
    }
    else
    {
        while (y0 != y1)
        {
            int tempx,tempy;
            tempx = x0 + ((sx-1)/2);
            tempy = y0 + ((sy-1)/2);

            f += dx;
            if (f >= dy)
            {
                if (tempx < m_cols && tempx > 0 && tempy < m_rows && tempy > 0)
                    if (Grid(GetNode(tempx,tempy)))
                        return false;

                x0 += sx;
                f -= dy;
            }

            if (tempx < m_cols && tempx > 0 && tempy < m_rows && tempy > 0)
                if (f != 0 && Grid(GetNode(tempx, tempy)))
                    return false;

            if (x0 > 0 && tempy < m_rows && tempy > 0)
                if (dx == 0 && Grid(GetNode(x0,tempy)) && Grid(GetNode(x0-1,tempy)))
                    return false;


            y0 += sy;
        }
    }
    return true;
}// END FUNCTION LOS(FRINGE_VIS S, FINGE_VIS SP)


/*! Get a successor node from index as specified by direction
 *
 *  @param          index           Index the the node to expand
 *  @param          direction       Direction of node to expand
 *  @param[out]     sind            Successor returned index
 *  @ICPATHNODE     ICPATHNODE      Copy of the pathing node
**/
ICPATHNODE VisAstar::Successor(int index, uchar direction, int* sind)
{
    int x,y;
    GetCoord(index,&x,&y);

    if (direction&ASTR_E)
    {
        if (x < m_cols-1)
        {
            *sind = (y*m_cols) + x + 1;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_N)
    {
        if (y < m_rows-1)
        {
            *sind = ((y+1)*m_cols) + x;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_NE)
    {
        if (y < m_rows-1 && x < m_cols-1)
        {
            *sind = ((y+1)*m_cols) + x + 1;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_NW)
    {
        if (x > 0 && y < m_rows-1)
        {
            *sind = ((y+1)*m_cols) + x - 1;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_W)
    {
        if (x > 0)
        {
            *sind = (y*m_cols) + x - 1;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_SW)
    {
        if (x > 0 && y > 0)
        {
            *sind = ((y-1)*m_cols) + x - 1;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_S)
    {
        if (y > 0)
        {
            *sind = ((y-1)*m_cols) + x;
            return m_Data[*sind];
        }
        return 0;
    }
    if (direction&ASTR_SE)
    {
        if (x < m_cols-1 && y > 0)
        {
            *sind = ((y-1)*m_cols) + x + 1;
            return m_Data[*sind];
        }
        return 0;
    }
    return 0;
}// END FUNCTION Successor(int index, uchar direction)


/*! Called to adjust the links to neighbors if a node is modified
 *
 *  @param      x           X position of node that was modified
 *  @param      y           Y position of node that was modified
**/
void VisAstar::FixNeighbors(int x, int y)
{
    int w = x-1;
    int e = x+1;
    int n = y+1;
    int s = y-1;

    ICPATHNODE node = GetNode(x,y);

    VERT_FLAT_TEX_2D* pvb = (VERT_FLAT_TEX_2D*)m_Quads.Lock();

    int ind = (y * m_cols + x) * 4;

    float u0 = (float)(node%16)/16.0f;
    float u1 = u0 + 1.0f/16.0f;
    float v0 = (float)(node/16)/16.0f;
    float v1 = v0 + 1.0f/16.0f;

    pvb[ind+0].uv.Set(u0,v0);
    pvb[ind+1].uv.Set(u0,v1);
    pvb[ind+2].uv.Set(u1,v1);
    pvb[ind+3].uv.Set(u1,v0);

    // w
    if (x > 0)
    {
        ICPATHNODE& temp = GetNode(w,y);
        if (node&ASTR_W)
            temp |= ASTR_E;
        else
            temp &= ~ASTR_E;

        ind = (y * m_cols + w) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // sw    
    if (x > 0 && y > 0)
    {
        ICPATHNODE& temp = GetNode(w,s);
        //temp &= ((node&ASTR_SW) ? ASTR_NE : ~ASTR_NE);
        if (node&ASTR_SW)
            temp |= ASTR_NE;
        else
            temp &= ~ASTR_NE;

        ind = (s * m_cols + w) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // nw
    if (x > 0 && y < m_rows-1)
    {
        ICPATHNODE& temp = GetNode(w,n);
        //temp &= ((node&ASTR_NW) ? ASTR_SE : ~ASTR_SE);
        if (node&ASTR_NW)
            temp |= ASTR_SE;
        else
            temp &= ~ASTR_SE;

        ind = (n * m_cols + w) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // s
    if (y > 0)
    {
        ICPATHNODE& temp = GetNode(x,s);
        //temp &= ((node&ASTR_S) ? ASTR_N : ~ASTR_N);
        if (node&ASTR_S)
            temp |= ASTR_N;
        else
            temp &= ~ASTR_N;

        ind = (s * m_cols + x) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // n
    if (y < m_rows-1)
    {
        ICPATHNODE& temp = GetNode(x,n);
        //temp &= ((node&ASTR_N) ? ASTR_S : ~ASTR_S);
        if (node&ASTR_N)
            temp |= ASTR_S;
        else
            temp &= ~ASTR_S;

        ind = (n * m_cols + x) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // ne
    if (y < m_rows-1 && x < m_cols-1)
    {
        ICPATHNODE& temp = GetNode(e,n);
        //temp &= ((node&ASTR_NE) ? ASTR_SW : ~ASTR_SW);
        if (node&ASTR_NE)
            temp |= ASTR_SW;
        else
            temp &= ~ASTR_SW;

        ind = (n * m_cols + e) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // e
    if (x < m_cols-1)
    {
        ICPATHNODE& temp = GetNode(e,y);
        //temp &= ((node&ASTR_E) ? ASTR_W : ~ASTR_W);
        if (node&ASTR_E)
            temp |= ASTR_W;
        else
            temp &= ~ASTR_W;

        ind = (y * m_cols + e) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    // se
    if (x < m_cols-1 && y > 0)
    {
        ICPATHNODE& temp = GetNode(e,s);
        //temp &= ((node&ASTR_SE) ? ASTR_NW : ~ASTR_NW);
        if (node&ASTR_SE)
            temp |= ASTR_NW;
        else
            temp &= ~ASTR_NW;

        ind = (s * m_cols + e) * 4;

        float u0 = (float)(temp%16)/16.0f;
        float u1 = u0 + 1.0f/16.0f;
        float v0 = (float)(temp/16)/16.0f;
        float v1 = v0 + 1.0f/16.0f;

        pvb[ind+0].uv.Set(u0,v0);
        pvb[ind+1].uv.Set(u0,v1);
        pvb[ind+2].uv.Set(u1,v1);
        pvb[ind+3].uv.Set(u1,v0);
    }

    m_Quads.Unlock();

    FixVis(x,y);
}// END FUNCTION FixNeighbors(uint x, uint y)


/*! Gets the x,y coords of a node by its index
 *
 *  @param          index           Index into m_Data
 *  @param          x               Pointer to store x position
 *  @param          y               Pointer to store y position
**/
void VisAstar::GetCoord(int index, int* x, int* y)
{
    *x = index % m_cols;
    *y = index/m_cols;
}// END FUNCTION GetCoord(int index, int* x, int* y)


/*! Asks if the given index exists in the closed list
 *
 *      if it does, returns true and fills in some heuristic information
 *
 *  @param          index           Index to check
 *  @param[out]     ph              Pointer to store H
 *  @param[out]     pg              Pointer to store G
 *  @returns        bool            true on success
**/
bool VisAstar::GetClosed(int index, int* ph, int* pg)
{
    FRINGE_VIS temp;
    temp.index = m_Data2[index];
    int i = icMinHeapFind(m_Closed, m_numClosed, temp);
    if (i >= 0)
    {
        FRINGE_VIS el = m_Closed[i];
        *ph = el.H;
        *pg = el.G;
        return true;
    }

    return false;
}// END FUNCTION GetClosed(int index, int* ph, int* pg)


/*! Builds the visibility graph
 *
 *  (we keep the astar data in tact)
**/
void VisAstar::BuildVis()
{
    for (int i=0; i<m_cols; ++i)
    {
        for (int j=0; j<m_rows; ++j)
        {
            for (int x=0; x<m_cols; ++x)
            {
                for (int y=0; y<m_rows; ++y)
                {
                    if (i != x || y != j)
                    {
                        FRINGE_VIS S,SP;
                        S.index = m_Data2[j*m_cols+i];

                        SP.index = m_Data2[y*m_cols+x];
                        if (LOS(S,SP))
                        {
                            if (!S.index->pHead)
                            {
                                S.index->pHead = new VisNode();
                                S.index->pHead->path_index = SP.index->path_index;
                            }
                            else
                            {
                                VisNode* temp = S.index->pHead->pNext;
                                S.index->pHead = new VisNode();
                                S.index->pHead->path_index = SP.index->path_index;
                                S.index->pHead->pNext = temp;
                            }
                        }
                        if (LOS(SP,S))
                        {
                            if (!SP.index->pHead)
                            {
                                SP.index->pHead = new VisNode();
                                SP.index->pHead->path_index = S.index->path_index;
                            }
                            else
                            {
                                VisNode* temp = SP.index->pHead->pNext;
                                SP.index->pHead = new VisNode();
                                SP.index->pHead->path_index = S.index->path_index;
                                SP.index->pHead->pNext = temp;
                            }
                        }
                    }
                }
            }
        }
    }
}// END FUNCTION BuildVis()


/*! Fixes neighbor links in the visibility graph after a change
 *
 *  @param          x           X index of node that changed
 *  @param          y           Y index of node that changed
**/
void VisAstar::FixVis(int x, int y)
{
    //int ind = (y * m_cols + x) * 4;

    //// recheck all the nodes this one claims it can see
    //VisNode* node = m_Data2[ind];
    //while (node)
    //{
    //}
}// END FUNCTION FixVis(int x, int y)

void VisAstar::CheckData()
{
    BuildVis();
}

#endif//#if 0