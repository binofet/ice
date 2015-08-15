#ifndef __AIRHOCKEY_SETTINGS_H__
#define __AIRHOCKEY_SETTINGS_H__


#include "Core/GXDevice/icGXDevice.h"


/*! Defines the settings available for an airhockey game
**/
struct SkeeBallSettings
{
    SkeeBallSettings()
    {
        bFullscreen = false;

        bFog = true;
        bMusic = true;
        bSfx = true;

        bFreeCam = true;
        bWiiMote = false;

        fMusic_vol = 0.75f;
        fSfx_vol = 0.5f;

        fogParams.col.Set(240,156,46,255);         //col
        fogParams.density = 0.000001f;      //density
        fogParams.end = 125.000000f;                  //end
        fogParams.start = 20.000000f;                //start
        fogParams.type = 1;                     //type

        iScreen_x = 1280;       //! X window resolution
        iScreen_y = 720;       //! Y window resolution
        iHighScore = 0;
    };

    bool operator==(const SkeeBallSettings& Rhs)
    {
        return (bWiiMote == Rhs.bWiiMote &&
                bFreeCam == Rhs.bFreeCam &&
                bFullscreen == Rhs.bFullscreen &&
                bFog == Rhs.bFog &&
                bMusic == Rhs.bMusic &&
                bSfx == Rhs.bSfx &&
                fMusic_vol == Rhs.fMusic_vol &&
                fSfx_vol == Rhs.fSfx_vol &&
                fogParams == Rhs.fogParams &&
                iScreen_x == Rhs.iScreen_x &&
                iScreen_y == Rhs.iScreen_y &&
                iHighScore == Rhs.iHighScore);
    };

    bool operator!=(const SkeeBallSettings& Rhs)
    {
        return (bWiiMote != Rhs.bWiiMote ||
                bFreeCam != Rhs.bFreeCam ||
                bFullscreen != Rhs.bFullscreen ||
                bFog != Rhs.bFog ||
                bMusic != Rhs.bMusic ||
                bSfx != Rhs.bSfx ||
                fMusic_vol != Rhs.fMusic_vol ||
                fSfx_vol != Rhs.fSfx_vol ||
                fogParams != Rhs.fogParams ||
                iScreen_x != Rhs.iScreen_x ||
                iScreen_y != Rhs.iScreen_y ||
                iHighScore != Rhs.iHighScore);
    };

    bool        bFullscreen;     //! Full screen application
    bool        bFog;            //! Fog On/Off
    bool        bMusic;          //! Music On/Off
    bool        bSfx;            //! Sound effects On/Off

    bool        bFreeCam;
    bool        bWiiMote;

    float       fMusic_vol;      //! Music volume [0.0f,1.0f]
    float       fSfx_vol;        //! Sound effects volume [0.0f, 1.0f]

    icFogParams fogParams;       //! Fog params

    int         iScreen_x;       //! X window resolution
    int         iScreen_y;       //! Y window resolution
    int         iHighScore;
};

#endif //__AIRHOCKEY_SETTINGS_H__