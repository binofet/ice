#pragma once
#include "native_context.h"
#include <GL/glcorearb.h>
namespace ice::render::gl::detail {
struct functions {
	PFNGLGETSTRINGPROC               GetString               = nullptr;
	PFNGLGETINTEGERVPROC             GetIntegerv             = nullptr;
	PFNGLGETERRORPROC                GetError                = nullptr;
	PFNGLGENVERTEXARRAYSPROC         GenVertexArrays         = nullptr;
	PFNGLBINDVERTEXARRAYPROC         BindVertexArray         = nullptr;
	PFNGLDELETEVERTEXARRAYSPROC      DeleteVertexArrays      = nullptr;
	PFNGLGENBUFFERSPROC              GenBuffers              = nullptr;
	PFNGLBINDBUFFERPROC              BindBuffer              = nullptr;
	PFNGLBUFFERDATAPROC              BufferData              = nullptr;
	PFNGLBINDBUFFERBASEPROC          BindBufferBase          = nullptr;
	PFNGLDELETEBUFFERSPROC           DeleteBuffers           = nullptr;
	PFNGLCREATESHADERPROC            CreateShader            = nullptr;
	PFNGLSHADERSOURCEPROC            ShaderSource            = nullptr;
	PFNGLCOMPILESHADERPROC           CompileShader           = nullptr;
	PFNGLGETSHADERIVPROC             GetShaderiv             = nullptr;
	PFNGLGETSHADERINFOLOGPROC        GetShaderInfoLog        = nullptr;
	PFNGLDELETESHADERPROC            DeleteShader            = nullptr;
	PFNGLCREATEPROGRAMPROC           CreateProgram           = nullptr;
	PFNGLATTACHSHADERPROC            AttachShader            = nullptr;
	PFNGLLINKPROGRAMPROC             LinkProgram             = nullptr;
	PFNGLGETPROGRAMIVPROC            GetProgramiv            = nullptr;
	PFNGLGETACTIVEATTRIBPROC         GetActiveAttrib         = nullptr;
	PFNGLGETATTRIBLOCATIONPROC       GetAttribLocation       = nullptr;
	PFNGLGETPROGRAMINFOLOGPROC       GetProgramInfoLog       = nullptr;
	PFNGLDELETEPROGRAMPROC           DeleteProgram           = nullptr;
	PFNGLUSEPROGRAMPROC              UseProgram              = nullptr;
	PFNGLGETACTIVEUNIFORMBLOCKIVPROC GetActiveUniformBlockiv = nullptr;
	PFNGLUNIFORMBLOCKBINDINGPROC     UniformBlockBinding     = nullptr;
	PFNGLGETACTIVEUNIFORMSIVPROC     GetActiveUniformsiv     = nullptr;
	PFNGLGENTEXTURESPROC             GenTextures             = nullptr;
	PFNGLBINDTEXTUREPROC             BindTexture             = nullptr;
	PFNGLTEXIMAGE2DPROC              TexImage2D              = nullptr;
	PFNGLTEXPARAMETERIPROC           TexParameteri           = nullptr;
	PFNGLDELETETEXTURESPROC          DeleteTextures          = nullptr;
	PFNGLGENFRAMEBUFFERSPROC         GenFramebuffers         = nullptr;
	PFNGLBINDFRAMEBUFFERPROC         BindFramebuffer         = nullptr;
	PFNGLFRAMEBUFFERTEXTURE2DPROC    FramebufferTexture2D    = nullptr;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC  CheckFramebufferStatus  = nullptr;
	PFNGLDELETEFRAMEBUFFERSPROC      DeleteFramebuffers      = nullptr;
	PFNGLVIEWPORTPROC                Viewport                = nullptr;
	PFNGLCLEARCOLORPROC              ClearColor              = nullptr;
	PFNGLCLEARPROC                   Clear                   = nullptr;
	PFNGLDRAWARRAYSPROC              DrawArrays              = nullptr;
	PFNGLPIXELSTOREIPROC             PixelStorei             = nullptr;
	PFNGLREADPIXELSPROC              ReadPixels              = nullptr;
	PFNGLDISABLEPROC                 Disable                 = nullptr;
	PFNGLFINISHPROC                  Finish                  = nullptr;
	PFNGLFLUSHPROC                   Flush                   = nullptr;
	bool                             load(native_context &context, std::string &error) {
        GetString = reinterpret_cast<PFNGLGETSTRINGPROC>(context.resolve("glGetString"));
        if (!GetString) {
            error = "Missing OpenGL entry point glGetString";
            return false;
        }
        GetIntegerv = reinterpret_cast<PFNGLGETINTEGERVPROC>(context.resolve("glGetIntegerv"));
        if (!GetIntegerv) {
            error = "Missing OpenGL entry point glGetIntegerv";
            return false;
        }
        GetError = reinterpret_cast<PFNGLGETERRORPROC>(context.resolve("glGetError"));
        if (!GetError) {
            error = "Missing OpenGL entry point glGetError";
            return false;
        }
        GenVertexArrays =
            reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(context.resolve("glGenVertexArrays"));
        if (!GenVertexArrays) {
            error = "Missing OpenGL entry point glGenVertexArrays";
            return false;
        }
        BindVertexArray =
            reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(context.resolve("glBindVertexArray"));
        if (!BindVertexArray) {
            error = "Missing OpenGL entry point glBindVertexArray";
            return false;
        }
        DeleteVertexArrays =
            reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(context.resolve("glDeleteVertexArrays"));
        if (!DeleteVertexArrays) {
            error = "Missing OpenGL entry point glDeleteVertexArrays";
            return false;
        }
        GenBuffers = reinterpret_cast<PFNGLGENBUFFERSPROC>(context.resolve("glGenBuffers"));
        if (!GenBuffers) {
            error = "Missing OpenGL entry point glGenBuffers";
            return false;
        }
        BindBuffer = reinterpret_cast<PFNGLBINDBUFFERPROC>(context.resolve("glBindBuffer"));
        if (!BindBuffer) {
            error = "Missing OpenGL entry point glBindBuffer";
            return false;
        }
        BufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(context.resolve("glBufferData"));
        if (!BufferData) {
            error = "Missing OpenGL entry point glBufferData";
            return false;
        }
        BindBufferBase =
            reinterpret_cast<PFNGLBINDBUFFERBASEPROC>(context.resolve("glBindBufferBase"));
        if (!BindBufferBase) {
            error = "Missing OpenGL entry point glBindBufferBase";
            return false;
        }
        DeleteBuffers =
            reinterpret_cast<PFNGLDELETEBUFFERSPROC>(context.resolve("glDeleteBuffers"));
        if (!DeleteBuffers) {
            error = "Missing OpenGL entry point glDeleteBuffers";
            return false;
        }
        CreateShader = reinterpret_cast<PFNGLCREATESHADERPROC>(context.resolve("glCreateShader"));
        if (!CreateShader) {
            error = "Missing OpenGL entry point glCreateShader";
            return false;
        }
        ShaderSource = reinterpret_cast<PFNGLSHADERSOURCEPROC>(context.resolve("glShaderSource"));
        if (!ShaderSource) {
            error = "Missing OpenGL entry point glShaderSource";
            return false;
        }
        CompileShader =
            reinterpret_cast<PFNGLCOMPILESHADERPROC>(context.resolve("glCompileShader"));
        if (!CompileShader) {
            error = "Missing OpenGL entry point glCompileShader";
            return false;
        }
        GetShaderiv = reinterpret_cast<PFNGLGETSHADERIVPROC>(context.resolve("glGetShaderiv"));
        if (!GetShaderiv) {
            error = "Missing OpenGL entry point glGetShaderiv";
            return false;
        }
        GetShaderInfoLog =
            reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(context.resolve("glGetShaderInfoLog"));
        if (!GetShaderInfoLog) {
            error = "Missing OpenGL entry point glGetShaderInfoLog";
            return false;
        }
        DeleteShader = reinterpret_cast<PFNGLDELETESHADERPROC>(context.resolve("glDeleteShader"));
        if (!DeleteShader) {
            error = "Missing OpenGL entry point glDeleteShader";
            return false;
        }
        CreateProgram =
            reinterpret_cast<PFNGLCREATEPROGRAMPROC>(context.resolve("glCreateProgram"));
        if (!CreateProgram) {
            error = "Missing OpenGL entry point glCreateProgram";
            return false;
        }
        AttachShader = reinterpret_cast<PFNGLATTACHSHADERPROC>(context.resolve("glAttachShader"));
        if (!AttachShader) {
            error = "Missing OpenGL entry point glAttachShader";
            return false;
        }
        LinkProgram = reinterpret_cast<PFNGLLINKPROGRAMPROC>(context.resolve("glLinkProgram"));
        if (!LinkProgram) {
            error = "Missing OpenGL entry point glLinkProgram";
            return false;
        }
        GetProgramiv = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(context.resolve("glGetProgramiv"));
        if (!GetProgramiv) {
            error = "Missing OpenGL entry point glGetProgramiv";
            return false;
        }
        GetActiveAttrib =
            reinterpret_cast<PFNGLGETACTIVEATTRIBPROC>(context.resolve("glGetActiveAttrib"));
        if (!GetActiveAttrib) {
            error = "Missing OpenGL entry point glGetActiveAttrib";
            return false;
        }
        GetAttribLocation =
            reinterpret_cast<PFNGLGETATTRIBLOCATIONPROC>(context.resolve("glGetAttribLocation"));
        if (!GetAttribLocation) {
            error = "Missing OpenGL entry point glGetAttribLocation";
            return false;
        }
        GetProgramInfoLog =
            reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(context.resolve("glGetProgramInfoLog"));
        if (!GetProgramInfoLog) {
            error = "Missing OpenGL entry point glGetProgramInfoLog";
            return false;
        }
        DeleteProgram =
            reinterpret_cast<PFNGLDELETEPROGRAMPROC>(context.resolve("glDeleteProgram"));
        if (!DeleteProgram) {
            error = "Missing OpenGL entry point glDeleteProgram";
            return false;
        }
        UseProgram = reinterpret_cast<PFNGLUSEPROGRAMPROC>(context.resolve("glUseProgram"));
        if (!UseProgram) {
            error = "Missing OpenGL entry point glUseProgram";
            return false;
        }
        GetActiveUniformBlockiv = reinterpret_cast<PFNGLGETACTIVEUNIFORMBLOCKIVPROC>(
            context.resolve("glGetActiveUniformBlockiv"));
        if (!GetActiveUniformBlockiv) {
            error = "Missing OpenGL entry point glGetActiveUniformBlockiv";
            return false;
        }
        UniformBlockBinding = reinterpret_cast<PFNGLUNIFORMBLOCKBINDINGPROC>(
            context.resolve("glUniformBlockBinding"));
        if (!UniformBlockBinding) {
            error = "Missing OpenGL entry point glUniformBlockBinding";
            return false;
        }
        GetActiveUniformsiv = reinterpret_cast<PFNGLGETACTIVEUNIFORMSIVPROC>(
            context.resolve("glGetActiveUniformsiv"));
        if (!GetActiveUniformsiv) {
            error = "Missing OpenGL entry point glGetActiveUniformsiv";
            return false;
        }
        GenTextures = reinterpret_cast<PFNGLGENTEXTURESPROC>(context.resolve("glGenTextures"));
        if (!GenTextures) {
            error = "Missing OpenGL entry point glGenTextures";
            return false;
        }
        BindTexture = reinterpret_cast<PFNGLBINDTEXTUREPROC>(context.resolve("glBindTexture"));
        if (!BindTexture) {
            error = "Missing OpenGL entry point glBindTexture";
            return false;
        }
        TexImage2D = reinterpret_cast<PFNGLTEXIMAGE2DPROC>(context.resolve("glTexImage2D"));
        if (!TexImage2D) {
            error = "Missing OpenGL entry point glTexImage2D";
            return false;
        }
        TexParameteri =
            reinterpret_cast<PFNGLTEXPARAMETERIPROC>(context.resolve("glTexParameteri"));
        if (!TexParameteri) {
            error = "Missing OpenGL entry point glTexParameteri";
            return false;
        }
        DeleteTextures =
            reinterpret_cast<PFNGLDELETETEXTURESPROC>(context.resolve("glDeleteTextures"));
        if (!DeleteTextures) {
            error = "Missing OpenGL entry point glDeleteTextures";
            return false;
        }
        GenFramebuffers =
            reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(context.resolve("glGenFramebuffers"));
        if (!GenFramebuffers) {
            error = "Missing OpenGL entry point glGenFramebuffers";
            return false;
        }
        BindFramebuffer =
            reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(context.resolve("glBindFramebuffer"));
        if (!BindFramebuffer) {
            error = "Missing OpenGL entry point glBindFramebuffer";
            return false;
        }
        FramebufferTexture2D = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(
            context.resolve("glFramebufferTexture2D"));
        if (!FramebufferTexture2D) {
            error = "Missing OpenGL entry point glFramebufferTexture2D";
            return false;
        }
        CheckFramebufferStatus = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(
            context.resolve("glCheckFramebufferStatus"));
        if (!CheckFramebufferStatus) {
            error = "Missing OpenGL entry point glCheckFramebufferStatus";
            return false;
        }
        DeleteFramebuffers =
            reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(context.resolve("glDeleteFramebuffers"));
        if (!DeleteFramebuffers) {
            error = "Missing OpenGL entry point glDeleteFramebuffers";
            return false;
        }
        Viewport = reinterpret_cast<PFNGLVIEWPORTPROC>(context.resolve("glViewport"));
        if (!Viewport) {
            error = "Missing OpenGL entry point glViewport";
            return false;
        }
        ClearColor = reinterpret_cast<PFNGLCLEARCOLORPROC>(context.resolve("glClearColor"));
        if (!ClearColor) {
            error = "Missing OpenGL entry point glClearColor";
            return false;
        }
        Clear = reinterpret_cast<PFNGLCLEARPROC>(context.resolve("glClear"));
        if (!Clear) {
            error = "Missing OpenGL entry point glClear";
            return false;
        }
        DrawArrays = reinterpret_cast<PFNGLDRAWARRAYSPROC>(context.resolve("glDrawArrays"));
        if (!DrawArrays) {
            error = "Missing OpenGL entry point glDrawArrays";
            return false;
        }
        PixelStorei = reinterpret_cast<PFNGLPIXELSTOREIPROC>(context.resolve("glPixelStorei"));
        if (!PixelStorei) {
            error = "Missing OpenGL entry point glPixelStorei";
            return false;
        }
        ReadPixels = reinterpret_cast<PFNGLREADPIXELSPROC>(context.resolve("glReadPixels"));
        if (!ReadPixels) {
            error = "Missing OpenGL entry point glReadPixels";
            return false;
        }
        Disable = reinterpret_cast<PFNGLDISABLEPROC>(context.resolve("glDisable"));
        if (!Disable) {
            error = "Missing OpenGL entry point glDisable";
            return false;
        }
        Finish = reinterpret_cast<PFNGLFINISHPROC>(context.resolve("glFinish"));
        if (!Finish) {
            error = "Missing OpenGL entry point glFinish";
            return false;
        }
        Flush = reinterpret_cast<PFNGLFLUSHPROC>(context.resolve("glFlush"));
        if (!Flush) {
            error = "Missing OpenGL entry point glFlush";
            return false;
        }
        return true;
	}
};
} // namespace ice::render::gl::detail
