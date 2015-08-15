#ifndef __IC_2D_COLLISION_H__
#define __IC_2D_COLLISION_H__

#include "Math/Vector/icVector2.h"


/*! 2D collision information
**/
struct ic2DCollisionInfo
{
    icVector2   hitPoint;
	icVector2   normal;
	icReal      depth;
};


/*! 2D Circle Shape
**/
struct ic2DCircle
{
    icVector2   pos;
    icReal      radius;
};


/*! 2D Axis Aligned Bounding Box
**/
struct ic2DAABB
{
    icReal GetRadius(icReal theta) const
    {
        icReal alpha = icAtan2(extents.y,extents.x);
        if (theta < alpha || theta >= (2.0f * IC_PI - alpha))
            return extents.x / icCos(theta);
        if (theta < (IC_PI - alpha))
            return extents.y / icCos(theta - IC_PI_HALF);
        if (theta < (IC_PI + alpha))
            return extents.x / icCos(theta - IC_PI);

        return extents.x / icCos(theta - (3.0f*IC_PI_HALF));
    };

    icVector2   pos;
    icVector2   extents;
};


/*! 2D Object Oriented Bounding Box
**/
struct ic2DOOBB
{
    icVector2   pos;
    icVector2   extents;
    icReal      rot;
};


/*! Determines if point is inside AABB
 *
 *  @param          p           Point to test versus box
 *  @param          box         Box to test versus point
 *  @returns        bool        true if point is inside box
**/
FORCE_INLINE bool ic2DPointAABB(const icVector2& p, const ic2DAABB& box)
{
    if (p.x > box.pos.x + box.extents.x)
        return false;
    if (p.x < box.pos.x - box.extents.x)
        return false;
    if (p.y > box.pos.y + box.extents.y)
        return false;
    if (p.y < box.pos.y - box.extents.y)
        return false;
    return true;
};// END FUNCTION ic2DPointAABB(const icVector2& p, const ic2DAABB box)


/*! Determines if point is inside circle
 *
 *  @param          p           Poing to test versus circle
 *  @param          circle      Circle to test versus point
 *  @returns        bool        true if point is inside circle
**/
FORCE_INLINE bool ic2DPointCircle(const icVector2& p,
                                  const ic2DCircle& circle)
{
    return (p.Distance(circle.pos) <= circle.radius);
};// END FUNCTION ic2DPointCircle(const icVector2& p,const ic2DCircle& circle)


/*! Intersect test for circle + axis aligned box
 *
 *  @param      circle      Position and radius
 *  @param      box         Position and extents
 *  @param[out] pInfo       Pointer to CollisionInfo struct
 *  @returns    bool        True if the objects are in collision
**/
FORCE_INLINE bool ic2DCircleAABox(const ic2DCircle& circle,
                                  const ic2DAABB& box,
                                  ic2DCollisionInfo* pInfo)
{
    icVector2 vc2b = box.pos - circle.pos;
    icReal dist_sqr = vc2b.LengthSqr();   //! dist2 between objects
    icReal ang_between = icAtan2(vc2b.y,vc2b.x); //!ang between objects

    // clamp to positive angles
    while (ang_between < 0.0f)
        ang_between += 2.0f * IC_PI;

    // check if box and circle collide
    icReal boxradius = box.GetRadius(ang_between);
    icReal opposit_ang = ang_between - IC_PI;

    // clamp to positive angles
    while (opposit_ang < 0.0f)
        opposit_ang += 2.0f * IC_PI;

    //icReal angle = ang_between * 180.0f / IC_PI;
    //icReal box_rad_sqr = boxradius*boxradius;
    icReal remaining = icSqrt(dist_sqr) - circle.radius - boxradius;


    // check corners first
    icVector2 corners[4];

    corners[0].Set(box.pos.x - box.extents.x, box.pos.y - box.extents.y);
    corners[1].Set(box.pos.x - box.extents.x, box.pos.y + box.extents.y);
    corners[2].Set(box.pos.x + box.extents.x, box.pos.y + box.extents.y);
    corners[3].Set(box.pos.x + box.extents.x, box.pos.y - box.extents.y);

    if (circle.pos.x < corners[0].x  && circle.pos.y < corners[0].y &&
        ic2DPointCircle(corners[0], circle))
    {
        // todo get the hit point
        pInfo->depth = abs(circle.radius - corners[0].Distance(circle.pos));
        pInfo->normal = (circle.pos - corners[0]);
        pInfo->normal.Normalize();
        return true;
    }
    else if (circle.pos.x < corners[1].x  && circle.pos.y > corners[1].y &&
             ic2DPointCircle(corners[1], circle))
    {
        // todo get the hit point
        pInfo->depth = abs(circle.radius - corners[1].Distance(circle.pos));
        pInfo->normal = (circle.pos - corners[1]).Normalize();
        return true;
    }
    else if (circle.pos.x > corners[2].x  && circle.pos.y > corners[2].y &&
             ic2DPointCircle(corners[2], circle))
    {
        // todo get the hit point
        pInfo->depth = abs(circle.radius - corners[2].Distance(circle.pos));
        pInfo->normal = (circle.pos - corners[2]).Normalize();
        return true;
    }
    else if (circle.pos.x > corners[3].x  && circle.pos.y < corners[3].y &&
             ic2DPointCircle(corners[3], circle))
    {
        // todo get the hit point
        pInfo->depth = abs(circle.radius - corners[3].Distance(circle.pos));
        pInfo->normal = (circle.pos - corners[3]).Normalize();
        return true;
    }


    // check edge planes
    if ( remaining <= 0.0f )
    {
        // todo get the hit point
        pInfo->depth = abs(remaining);
        pInfo->normal = circle.pos - box.pos;

        //if (abs(pInfo->normal.x) > abs(pInfo->normal.y))
        //    pInfo->normal.y = 0.0f;
        //else
        //    pInfo->normal.x = 0.0f;

        pInfo->normal.Normalize();

        return true;
    }

    return false;
}// END FUNCTION ic2DCircleAABox(const ic2DCircle& circle,
 //                              const ic2DAABB box,
 //                              ic2DCollisionInfo* pInfo)

#endif //__IC_2D_COLLISION_H__