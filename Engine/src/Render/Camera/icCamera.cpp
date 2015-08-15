
#include "Render/Camera/icCamera.h"

//#include "Core/GXDevice/icGLext.h"
//#include "GL/glu.h"

/*! Converts a world space position into screen space
 *
 *  @param              world           The position to convert
 *  @param              width           Width of screen
 *  @param              height          Height of screen
 *  @returns            icVector2       Screen position
**/
icVector2 icCamera::WorldToScreen(const icVector3& world, int width, int height)
{
	icVector2 screenPos;
    icVector4 v(0.0f,0.0f,0.0f,1.0f);
    icVector4 world4(world.x,world.y,world.z,1.0f);

    v = m_m44ViewMat * m_m44Projection * world4;

    screenPos.x = (v.x / v.w + 1.0f)/2.0f * (float)width;
    screenPos.y *= (-v.y / v.w + 1.0f)/2.0f * (float)height;

////Move source point into screen space
//Vector4 source = Vector4.Transform(position, SpacewarGame.Camera.View * SpacewarGame.Camera.Projection);
////and into pixels
//Vector2 source2D = new Vector2((int)((source.X / source.W + 1f) / 2f * 1280), (int)((-source.Y / source.W + 1f) / 2f * 720));



#ifdef IAMRETARDED
    int wind[4];

    GLdouble model[16];
    GLdouble proj[16];

    GLdouble x,y,z;

    icMatrix44 temp_mat = m_m44ViewMat.Inverse();
    for (int i=0; i<16; ++i)
    {
        model[i] = (GLdouble)temp_mat[i];
        proj[i] = (GLdouble)m_m44Projection[i];
    }

    glGetIntegerv(GL_VIEWPORT,wind);
    gluProject(world.x,world.y,world.z, model, proj, wind, &x,&y,&z);
#endif

    return screenPos;
}// END FUNCTION WorldToScreen(const icVector3& world, int width, int height)


/*! Converts a screen space position into a world direction
 *
 *  @param              v2Pos           The position to convert
 *  @param              v2Size          The screen size
 *  @returns            icVector3       World direction
**/
icVector3 icCamera::ScreenToWorldDir(const icVector2& v2Pos,
                                     const icVector2Int& v2Size) const
{
    icVector3 rayTo;
    rayTo.x = (((2.0f * v2Pos.x)/(icReal)v2Size.x) - 1)/m_m44Projection(0,0);
    rayTo.y = (((2.0f * v2Pos.y)/(icReal)v2Size.y) - 1)/m_m44Projection(1,1);
    rayTo.z = 1.0f;

    rayTo = m_m44ViewMat.Transform(rayTo.x,rayTo.y,rayTo.z);

    return rayTo;
}// END FUNCTION ScreenToWorldDir(const icVector2& screen)
