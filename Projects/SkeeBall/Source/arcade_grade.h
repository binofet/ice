#pragma once
#include "Render/Shader/icPipelineGL.h"

// Compatibility-profile post pass, deliberately independent of scene materials.
// The title and lobby stay ungraded for crisp type and reliably scannable QR.
class ArcadeGrade {
public:
    void Apply(int width,int height);
    void Release();
private:
    icProgramGL shader;
    GLuint texture=0;
    int w=0,h=0;
    bool attempted=false;
};
