#ifndef __IC_FONT_H__
#define __IC_FONT_H__

#include "Core/icGlobals.h"
#include "Core/GXDevice/icGXDevice.h"

#include "Math/Vector/icVector2Int.h"

struct icFontParams
{
	icString _fontName;
	int _size;
};


/*! Provides an interface for font objects which use operating specific
 *  and device specific constructs to create a renderable font.  i.e. 
 *  this interface is not to be used for bitmap fonts.
**/
class icFontI
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    virtual ICRESULT Initialize(const char* szFileName, const int size,
                                class icGXDevice* pDevice)=0;

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    virtual ICRESULT Print(const icVector2Int& v2ScreenPos, const icColor& color,
                           const char* buff)=0;

    virtual ICRESULT Printf(const icVector2Int& v2ScreenPos, const icColor& color,
                            const char* buff, ...)=0;

protected:
	class icGXDevice*       _gxDevice;
};

#endif // __IC_FONT_H__