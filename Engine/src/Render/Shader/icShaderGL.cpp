#include "Render/Shader/icShaderGL.h"
#include <vector>
#include <cstdlib>
#include <cstdint>

namespace {
bool available() {
#ifdef WIN32
#define LOAD(name,type) name = reinterpret_cast<type>(wglGetProcAddress(#name)); if(!name || reinterpret_cast<uintptr_t>(name)<=3 || reinterpret_cast<intptr_t>(name)==-1) return false
    LOAD(glCreateShader,PFNGLCREATESHADER); LOAD(glShaderSource,PFNGLSHADERSOURCE);
    LOAD(glCompileShader,PFNGLCOMPILESHADER); LOAD(glGetShaderiv,PFNGLGETSHADERIV);
    LOAD(glGetShaderInfoLog,PFNGLGETSHADERINFOLOG); LOAD(glDeleteShader,PFNGLDELETESHADER);
    LOAD(glCreateProgram,PFNGLCREATEPROGRAM); LOAD(glAttachShader,PFNGLATTACHSHADER);
    LOAD(glLinkProgram,PFNGLLINKPROGRAM); LOAD(glGetProgramiv,PFNGLGETPROGRAMIV);
    LOAD(glGetProgramInfoLog,PFNGLGETPROGRAMINFOLOG); LOAD(glDeleteProgram,PFNGLDELETEPROGRAM);
    LOAD(glUseProgram,PFNGLUSEPROGRAM); LOAD(glGetUniformLocation,PFNGLGETUNIFORMLOCATION);
    LOAD(glActiveTexture,PFNGLACTIVETEXTURE);
    LOAD(glUniform1i,PFNGLUNIFORM1I); LOAD(glUniform2f,PFNGLUNIFORM2F);
#undef LOAD
#endif
    const auto version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
    return version && std::atoi(version)>=2;
}
void logObject(GLuint object,bool shader,std::string& text) {
    GLint size=0;
    if(shader)glGetShaderiv(object,GL_INFO_LOG_LENGTH,&size);else glGetProgramiv(object,GL_INFO_LOG_LENGTH,&size);
    if(size>1){std::vector<char> log(size);if(shader)glGetShaderInfoLog(object,size,nullptr,log.data());else glGetProgramInfoLog(object,size,nullptr,log.data());text+=log.data();}
}
}
bool icShaderGL::Compile(const char* vertex,const char* fragment,std::string& diagnostics) {
    diagnostics.clear();
    if(!available()){diagnostics="GLSL 1.20 unavailable";return false;}
    GLuint stages[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
    const char* source[2]={vertex,fragment};bool ok=true;
    for(int i=0;i<2;++i){if(!stages[i]||!source[i]){ok=false;continue;}glShaderSource(stages[i],1,&source[i],nullptr);glCompileShader(stages[i]);GLint compiled=0;glGetShaderiv(stages[i],GL_COMPILE_STATUS,&compiled);logObject(stages[i],true,diagnostics);ok &= compiled!=0;}
    GLuint next=0;
    if(ok){next=glCreateProgram();if(next){for(auto s:stages)glAttachShader(next,s);glLinkProgram(next);GLint linked=0;glGetProgramiv(next,GL_LINK_STATUS,&linked);logObject(next,false,diagnostics);ok=linked!=0;}else ok=false;}
    for(auto s:stages)if(s)glDeleteShader(s);
    if(!ok){if(next)glDeleteProgram(next);return false;}
    Release();program=next;return true;
}
void icShaderGL::Release(){if(program){glDeleteProgram(program);program=0;}}
void icShaderGL::Bind() const {glUseProgram(program);}
void icShaderGL::Unbind(){glUseProgram(0);}
GLint icShaderGL::Uniform(const char* name) const{return program?glGetUniformLocation(program,name):-1;}
