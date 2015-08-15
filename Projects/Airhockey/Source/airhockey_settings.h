#ifndef __AIRHOCKEY_SETTINGS_H__
#define __AIRHOCKEY_SETTINGS_H__


#include "Core/GXDevice/icGXDevice.h"


/*! Defines the settings available for an airhockey game
**/
struct AirSettings
{
    AirSettings()
    {
        bFullscreen = false;

        bFog = true;
        bMusic = true;
        bSfx = true;

        fMusic_vol = 0.75f;
        fSfx_vol = 0.5f;

        fogParams.col.Set(240,156,46,255);         //col
        fogParams.density = 0.000001f;      //density
        fogParams.end = 125.000000f;                  //end
        fogParams.start = 20.000000f;                //start
        fogParams.type = 1;                     //type

        ucPaddle_type = 1;

        iScreen_x = 1280;       //! X window resolution
        iScreen_y = 720;       //! Y window resolution

        iBackbuf_x = 1280;      //! X back buffer size;
        iBackbuf_y = 720;      //! Y back buffer size;

        sLast_join_ip = "127.0.0.1";
    };

    bool operator==(const AirSettings& Rhs)
    {
        return (bFullscreen == Rhs.bFullscreen &&
                bFog == Rhs.bFog &&
                bMusic == Rhs.bMusic &&
                bSfx == Rhs.bSfx &&
                fMusic_vol == Rhs.fMusic_vol &&
                fSfx_vol == Rhs.fSfx_vol &&
                fogParams == Rhs.fogParams &&
                ucPaddle_type == Rhs.ucPaddle_type &&
                iScreen_x == Rhs.iScreen_x &&
                iScreen_y == Rhs.iScreen_y &&
                iBackbuf_x == Rhs.iBackbuf_x &&
                iBackbuf_y == Rhs.iBackbuf_y &&
                sLast_join_ip == Rhs.sLast_join_ip);
    };

    bool operator!=(const AirSettings& Rhs)
    {
        return (bFullscreen != Rhs.bFullscreen ||
                bFog != Rhs.bFog ||
                bMusic != Rhs.bMusic ||
                bSfx != Rhs.bSfx ||
                fMusic_vol != Rhs.fMusic_vol ||
                fSfx_vol != Rhs.fSfx_vol ||
                fogParams != Rhs.fogParams ||
                ucPaddle_type != Rhs.ucPaddle_type ||
                iScreen_x != Rhs.iScreen_x ||
                iScreen_y != Rhs.iScreen_y ||
                iBackbuf_x != Rhs.iBackbuf_x ||
                iBackbuf_y != Rhs.iBackbuf_y ||
                sLast_join_ip != Rhs.sLast_join_ip);
    };

    bool        bFullscreen;     //! Full screen application
    bool        bFog;            //! Fog On/Off
    bool        bMusic;          //! Music On/Off
    bool        bSfx;            //! Sound effects On/Off

    float       fMusic_vol;      //! Music volume [0.0f,1.0f]
    float       fSfx_vol;        //! Sound effects volume [0.0f, 1.0f]

    icFogParams fogParams;       //! Fog params

    uchar       ucPaddle_type;    //! Paddle Type (1-Circle,2-Triangle,3-Square)

    int         iScreen_x;       //! X window resolution
    int         iScreen_y;       //! Y window resolution

    int         iBackbuf_x;      //! X back buffer size;
    int         iBackbuf_y;      //! Y back buffer size;

    icString    sLast_join_ip;   //! The last ip joined "000.000.000.000"
};

#endif //__AIRHOCKEY_SETTINGS_H__