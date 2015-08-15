#include "Math/Matrix/icMatrix44.h"

icReal _ident44[] = {1.0f,0.0f,0.0f,0.0f,
                     0.0f,1.0f,0.0f,0.0f,
                     0.0f,0.0f,1.0f,0.0f,
                     0.0f,0.0f,0.0f,1.0f};

const icMatrix44 icMatrix44::IDENTITY(_ident44);