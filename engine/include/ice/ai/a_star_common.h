#pragma once

#include "ice/globals.h"
namespace ice::ai {

/// Packed navigation node storage used by the legacy pathfinding implementation.
typedef unsigned char pathnode;
/// union ICPATHNODE
//{
//	uint connectivityMask : 8;
//	uint heuristicIndex : 2;
//	uint debug : 2;
//	uint nodeInfoIndex : 10;
// };

#define ASTR_W 0x01
#define ASTR_NW 0x02
#define ASTR_N 0x04
#define ASTR_NE 0x08
#define ASTR_E 0x10
#define ASTR_SE 0x20
#define ASTR_S 0x40
#define ASTR_SW 0x80

#define MAX_SEARCH (8192)

#define INTDIST_OPT  //! USE int distances (scaled x 10000)
#define GIVEN_METHOD //! USE HEURISTICS FROM THE ASSIGNMENT

//! This doesn't seem to help at all!
// #define BREAK_TIES_W_G      //! (F1 == F2) ? return G1 <?> G2 : F1 <?> F2;

#define ROOT_2X10000 (14142) // 1.4142135623730950488016887242097
#define ROOT_2 (1.4142135623730950488016887242097f)

/// A-star frontier entry with node identity, accumulated cost, and estimated remaining cost.
struct fringe_element {
	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator<(const fringe_element &rhs) const {
#ifdef BREAK_TIES_W_G
#ifdef INDIST_OPT
		int f     = H + G;
		int rhs_f = Rhs.H + Rhs.G;
		if (f == rhs_f)
			return G < Rhs.G;
		return f < rhs_f;
#else
		float f     = H + G;
		float rhs_f = Rhs.H + Rhs.G;
		if (f == rhs_f)
			return G < Rhs.G;
		return f < rhs_f;
#endif
#else
		return h + g < rhs.h + rhs.g;
#endif
	};

	/// Compare the value/ordering represented by this type; references and resources remain owned
	/// by their original objects.
	bool operator>(const fringe_element &rhs) const {
#ifdef BREAK_TIES_W_G
#ifdef INDIST_OPT
		int f     = H + G;
		int rhs_f = Rhs.H + Rhs.G;
		if (f == rhs_f)
			return G > Rhs.G;
		return f > rhs_f;
#else
		float f     = H + G;
		float rhs_f = Rhs.H + Rhs.G;
		if (f == rhs_f)
			return G > Rhs.G;
		return f > rhs_f;
#endif
#else
		return h + g > rhs.h + rhs.g;
#endif
	};

	/// this is for the find function
	bool operator==(const fringe_element &rhs) const { return (index == rhs.index); };

	/// Parent node index; a negative value denotes a hierarchy root.
	int parent;
	/// First locked index element.
	int index;
#ifdef INTDIST_OPT
	/// Green channel as an unsigned byte in [0,255].
	int g;
	/// Height or heuristic cost in the units defined by the owning type.
	int h;
#else
	float G;
	float H;
#endif
};

} // namespace ice::ai
