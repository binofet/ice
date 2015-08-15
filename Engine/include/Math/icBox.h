#ifndef __IC_BOX_H__
#define __IC_BOX_H__

#include "Math/Vector/icVector2.h"
#include "Math/Matrix/icMatrix44.h"


/*! Provides an axis aligned bounding box object
**/
class icAABB
{
public:
	FORCE_INLINE icAABB(void)
	{
        max.Set(ICREAL_MIN,ICREAL_MIN);
        min.Set(ICREAL_MAX,ICREAL_MAX);
	};

    FORCE_INLINE void SetEmpty(void)
    {
        max.Set(ICREAL_MIN,ICREAL_MIN);
        min.Set(ICREAL_MAX,ICREAL_MAX);
    };

    /*! re-evaluate the aabb with new point
     *
     *      If the given point is outside the aabb, the aabb is expanded
     *
     *  @param      v2Point         New point to consider
     **/
	FORCE_INLINE void Add(const icVector2& v2Point)
	{
		if (v2Point.x > max.x)
			max.x = v2Point.x;
		if (v2Point.x < min.x)
			min.x = v2Point.x;
		if (v2Point.y > max.y)
			max.y = v2Point.y;
		if (v2Point.y < min.y)
			min.y = v2Point.y;
	};


    /*! re-evaluate the aabb with exisiting aabb
     *
     *      If the given aabb is outside the existing aabb,
     *      the aabb is expanded.
     *
     *  @param      aabb         New point to consider
    **/
	FORCE_INLINE void Add(const icAABB& aabb)
	{
		if (aabb.max.x > max.x)
			max.x = aabb.max.x;
		if (aabb.min.x < min.x)
			min.x = aabb.min.x;
		if (aabb.max.y > max.y)
			max.y = aabb.max.y;
		if (aabb.min.y < min.y)
			min.y = aabb.min.y;
	};


    /*  AABB - POINT intersection test
     *
     *  @param      v2Pos       Point to test intersection with
     *  @returns    bool        true if in intersection
    **/
    FORCE_INLINE bool Intersect(const icVector2& v2Pos)
    {
        // edge separation test
        if (max.x < v2Pos.x || min.x > v2Pos.x) return false;
        if (max.y < v2Pos.y || min.y > v2Pos.y) return false;

        return true;
    };


    /*  AABB - AABB intersection test
     *
     *  @param      aabb        AABB to test intersection with
     *  @returns    bool        true if in intersection
    **/
    FORCE_INLINE bool Intersect(const icAABB& aabb)
    {
        // edge separation test
        if (max.x < aabb.min.x || min.x > aabb.max.x) return false;
        if (max.y < aabb.min.y || min.y > aabb.max.y) return false;

        return true;
    };


    /*  AABB - OOBB intersection test
     *
     *  @param      oobb        OOBB to test intersection with
     *  @returns    bool        true if in intersection
    **/
    FORCE_INLINE bool Intersect(const class icOOBB& oobb)
    {
        return false;
    };

	icVector2	min;
	icVector2	max;
};


class icOOBB
{
public:
	//icOOBB(void)
	//{
	//	 
	//};


	//FORCE_INLINE void Add(const icVector2& v2Point)
	//{
	//};

	//FORCE_INLINE void Add(const icAABox& box);
	//{
	//};

	float		angle;
	icVector2	half_width;
};

#endif //__IC_BOX_H__