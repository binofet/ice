#include "stdio.h"
#include <math.h>
#include <memory.h>

#include "AI/Pathing/icAStar.h"
#include "Math/icRand.h"
#include "Memory/icMinHeap.h"

#define min(x, y) ( x < y ? x : y )
#define max(x, y) ( x > y ? x : y )

/*! c'tor
**/
icAStar::icAStar(void)
{
	m_Data = NULL;
	m_NumNodes = 0;
	m_rows = 0;
	m_cols = 0;

	m_numFringe = 0;
	m_numClosed = 0;
}// END FUNCTION icAStar(void)


/*! Cleans up all memories allocated
*
*  @returns        ICRESULT        Success/failure of cleanup
**/
ICRESULT icAStar::Cleanup(void)
{
	if (m_Data)
	{
		delete[] m_Data;
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
ICRESULT icAStar::Create(unsigned int x, unsigned int y, ICPATHNODE state)
{
	ICRESULT res = Cleanup();
	if (ICEFAIL(res))
		return res;

	m_rows = y;
	m_cols = x;
	m_NumNodes= x*y;

	m_Data = new ICPATHNODE[m_NumNodes];

	memset(m_Data,state,sizeof(ICPATHNODE)*m_NumNodes);

	if (!m_Data)
		return IC_FAIL_GEN;

	// TODO: DEBUG PATHING RENDERING
	//m_Quads.Create(VERT_TYPE_2D_FLAT_TEX,4*m_cols*m_rows,PRIM_TYPE_QUADS);

	//Texture* tex = Texture::LoadTGA("resource/links.tga");
	//if (!tex)
	//    return IC_FAIL_GEN;

	//m_Quads.SetTexture(tex,NULL);

	//VERT_FLAT_TEX_2D* pvb = (VERT_FLAT_TEX_2D*)m_Quads.Lock();

	//if (!pvb)
	//    return IC_FAIL_GEN;

	//icReal rx = XSIZE/(icReal)m_cols;
	//icReal ry = YSIZE/(icReal)m_rows;

	//int ind=0;
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

			//float xmin = (float)(i)*rx;
			//float xmax = (float)(i+1)*rx;
			//float ymin = (float)(j)*ry;
			//float ymax = (float)(j+1)*ry;

			//float u0 = (float)(temp%16)/16.0f;
			//float u1 = u0 + 0.98f/16.0f;
			//float v0 = (float)(temp/16)/16.0f;
			//float v1 = v0 + 0.99f/16.0f;

			//pvb[ind].uv.Set(u0,v0);
			//pvb[ind++].pos.Set(xmin,ymin);

			//pvb[ind].uv.Set(u0,v1);
			//pvb[ind++].pos.Set(xmin,ymax);

			//pvb[ind].uv.Set(u1,v1);
			//pvb[ind++].pos.Set(xmax,ymax);

			//pvb[ind].uv.Set(u1,v0);
			//pvb[ind++].pos.Set(xmax,ymin);

		}
	}

	//m_Quads.Unlock();


	return IC_OK;
}// END FUNCTION Create(unsigned int x, unsigned int y)


/*! Loads the pathing data from file
*
*  @param          szFile          File to load
*  @returns        ICRESULT        Success/failure of pathing load
**/
ICRESULT icAStar::Load(const char* szFile)
{
	ICRESULT res = Cleanup();
	if (ICEFAIL(res))
		return res;

	FILE* fp=NULL;

#if defined(_MSC_VER) && (_MSC_VER > 1400)
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
	Create(x,y,0x00);

	// TODO: really define these and serialize/deserialize them
	_worldMin.Set(0.0f, 0.0f, 0.0f);
	_worldMax.Set((icReal)x, (icReal)y, 0.0f);

	// read the pathing data in
	fread(m_Data,sizeof(ICPATHNODE),m_NumNodes,fp);

	for (int i=0; i<m_NumNodes; ++i)
	{
		x = i%m_cols;
		y = i/m_cols;
		FixNeighbors(x,y);
	}

	fclose(fp);

	return IC_OK;
}// END FUNCTION Load(const char* szFile)


/*! Saves the current pathing data to file
*
*  @param          szFile          File to save
*  @param          ICRESULT        Success/failure of pathing save
**/
ICRESULT icAStar::Save(const char* szFile)
{
	FILE* fp=NULL;

#if defined(_MSC_VER) && (_MSC_VER > 1400)
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
ICRESULT icAStar::Render(void)
{
	//m_Quads.Render();

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
bool icAStar::TryPlotRoute(
		const icVector3& startPosition,
		const icVector3& endPosition,
		icRouteBuffer& routeBuffer)
{
	uchar dir[8] = {0,1,2,3,4,5,6,7}; //! so that we can randomize the successor order

	uint GI;// = goaly*m_cols + goalx;      //! Goal index
	uint SI;// = starty*m_cols + startx;    //! Start index

	if (!TryGetNodeIndex(startPosition, &SI))
	{
		icWarning("Could not lookup world position");
		return false;
	}

	if (!TryGetNodeIndex(endPosition, &GI))
	{
		icWarning("Could not lookup world position");
		return false;
	}

	int startx = SI % m_cols;
	int starty = SI % m_rows;

	int goalx = GI % m_cols;
	int goaly = GI % m_rows;

	m_numFringe = 0;            //! Reset structures used in the search
	m_numClosed = 0;

	int dx = abs(goalx - startx);
	int dy = abs(goaly - starty);

	FRINGE_ELEMENT start;
	start.index = SI;
	start.parent = SI;
	start.G = 0;
#ifdef INTDIST_OPT
	start.H = ROOT_2X10000 * min(dx,dy) + 10000*max(dx,dy) - 10000*min(dx,dy);
#elif defined(GIVEN_METHOD)
	start.H = ROOT_2 * (float)min(dx,dy) + (float)max(dx,dy) - (float)min(dx,dy);
#else
	start.H = sqrtf((float)(dx*dx + dy*dy));
#endif

	m_Fringe[0] = start; m_numFringe++; //! Initialize fringe with start node

	// while there are nodes to explore...
	while (m_numFringe > 0)
	{
		FRINGE_ELEMENT S = icMinHeapDequeue(m_Fringe,m_numFringe--);

		// check indexes for goal node
		if (S.index == GI)
		{
			int num_nodes = 0;
			int index = S.parent;
			while (index != SI)
			{
				if (num_nodes < 1024)
				{
					//routeBuffer._indicies[num_nodes] = index;
					TryGetWorldPosition(index, routeBuffer._wayPoints[num_nodes]);
					// TODO hook up the other info
					num_nodes++;
				}
				FRINGE_ELEMENT temp;
				temp.index = index;
				int i = icMinHeapFind(m_Closed,m_numClosed,temp);
				if (i >= 0)
					index = m_Closed[i].parent;
				else
					icErrorf("Failed to find node in the heap!");
			}
			routeBuffer._size = num_nodes;
			return true;
		}

		// add S to the closed list, we've been here man!
		icMinHeapInsert(m_Closed,m_numClosed++,S);

		// randomize successor order
		for (int i=0; i<8; ++i)
			icSwap(dir[icRandI(0,7)],dir[icRandI(0,7)]);

		// go through the successors of S
		for (int i=0;i<8;++i)
		{
			uchar mask = (1<<dir[i]);

			FRINGE_ELEMENT s_prime;

			// get successor to S
			ICPATHNODE node = Successor(S.index,mask,&s_prime.index);

			if (!node)
				continue;

			int closed_index = icMinHeapFind(m_Closed,m_numClosed,s_prime);
			int fringe = icMinHeapFind(m_Fringe,m_numFringe,s_prime);
			if (closed_index < 0)
			{
				if (fringe < 0)
				{
#ifdef INTDIST_OPT
					s_prime.G = 0x7FFFFFFF;
#else
					s_prime.G = (float)0x7FFFFFFF;
#endif
					s_prime.parent = -1;
				}

				{
					int spx,spy;
					GetCoord(s_prime.index, &spx,&spy);

					int dx = abs(goalx - spx);
					int dy = abs(goaly - spy);

#ifdef INTDIST_OPT
					s_prime.H = ROOT_2X10000 * min(dx,dy) + 10000*max(dx,dy) - 10000*min(dx,dy);
#elif defined(GIVEN_METHOD)
					s_prime.H = ROOT_2 * (float)min(dx,dy) + (float)max(dx,dy) - (float)min(dx,dy);
#else
					s_prime.H = sqrtf((float)(dx*dx + dy*dy));
#endif
				}

				UpdateVertex(S,s_prime);
			}
		}
	}

	return false;
}// END FUNCTION PlotRoute(int startx, int starty, int goalx, int goaly,
//                        int* routeBuffer, int bufsize)


/*! Updates a vertex
*
*  @param          S           Vertex
*  @param          Sprime      Successor of S
**/
void icAStar::UpdateVertex(FRINGE_ELEMENT S, FRINGE_ELEMENT Sprime)
{
	int sx,sy,spx,spy;

	GetCoord(S.index, &sx,&sy);
	GetCoord(Sprime.index, &spx,&spy);

	int dx = abs(sx - spx);
	int dy = abs(sy - spy);

#ifdef INTDIST_OPT
	int COST = (dx & dy) ? ROOT_2X10000 : 10000;
#elif defined(GIVEN_METHOD)
	float COST = sqrtf((float)(dx*dx + dy*dy));
#else
	float COST = (dx & dy) ? ROOT_2 : 1.0f;
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
}// END FUNCTION UpdateVertex(FRINGE_ELEMENT S, FRINGE_ELEMENT Sprime)


/*! Get a successor node from index as specified by direction
*
*  @param          index           Index the the node to expand
*  @param          direction       Direction of node to expand
*  @param[out]     sind            Successor returned index
*  @ICPATHNODE     ICPATHNODE      Copy of the pathing node
**/
ICPATHNODE icAStar::Successor(int index, uchar direction, int* sind)
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
void icAStar::FixNeighbors(int x, int y)
{
	int w = x-1;
	int e = x+1;
	int n = y+1;
	int s = y-1;

	ICPATHNODE node = GetNode(x,y);

	//VERT_FLAT_TEX_2D* pvb = (VERT_FLAT_TEX_2D*)m_Quads.Lock();

	//int ind = (y * m_cols + x) * 4;

	//float u0 = (float)(node%16)/16.0f;
	//float u1 = u0 + 1.0f/16.0f;
	//float v0 = (float)(node/16)/16.0f;
	//float v1 = v0 + 1.0f/16.0f;

	//pvb[ind+0].uv.Set(u0,v0);
	//pvb[ind+1].uv.Set(u0,v1);
	//pvb[ind+2].uv.Set(u1,v1);
	//pvb[ind+3].uv.Set(u1,v0);

	// w
	if (x > 0)
	{
		ICPATHNODE& temp = GetNode(w,y);
		if (node&ASTR_W)
			temp |= ASTR_E;
		else
			temp &= ~ASTR_E;

		//ind = (y * m_cols + w) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (s * m_cols + w) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (n * m_cols + w) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (s * m_cols + x) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (n * m_cols + x) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (n * m_cols + e) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (y * m_cols + e) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
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

		//ind = (s * m_cols + e) * 4;

		//float u0 = (float)(temp%16)/16.0f;
		//float u1 = u0 + 1.0f/16.0f;
		//float v0 = (float)(temp/16)/16.0f;
		//float v1 = v0 + 1.0f/16.0f;

		//pvb[ind+0].uv.Set(u0,v0);
		//pvb[ind+1].uv.Set(u0,v1);
		//pvb[ind+2].uv.Set(u1,v1);
		//pvb[ind+3].uv.Set(u1,v0);
	}

	//m_Quads.Unlock();
}// END FUNCTION FixNeighbors(uint x, uint y)


/*! Gets the x,y coords of a node by its index
*
*  @param          index           Index into m_Data
*  @param          x               Pointer to store x position
*  @param          y               Pointer to store y position
**/
void icAStar::GetCoord(int index, int* x, int* y)
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
bool icAStar::GetClosed(int index, int* ph, int* pg)
{
	FRINGE_ELEMENT temp;
	temp.index = index;
	int i = icMinHeapFind(m_Closed, m_numClosed, temp);
	if (i >= 0)
	{
		FRINGE_ELEMENT el = m_Closed[i];
		*ph = el.H;
		*pg = el.G;
		return true;
	}

	return false;
}// END FUNCTION GetClosed(int index, int* ph, int* pg)