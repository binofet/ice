
//////////////////////////////////////////////////////////////////////////////
// SYSTEM INCLUDES
#ifdef WIN32
#include <windows.h>
#include <gl/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#include <Agl/agl.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif


#ifdef WIN32
#ifndef STDCALL
#define STDCALL __stdcall
#endif
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#else
#ifndef STDCALL
#define STDCALL
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#endif

extern bool IsExtensionSupported(const char* szTargetExt, const char* szExtList);

typedef void (APIENTRY * SWAPINTEXT) (GLint interval);
extern SWAPINTEXT wglSwapIntervalEXT;

/* ---------------------------- GL_VERSION_1_2 ---------------------------- */
#ifndef GL_VERSION_1_2
#define GL_VERSION_1_2
#define IC_GL_VERSION_1_2

#define GL_SMOOTH_POINT_SIZE_RANGE 0x0B12
#define GL_SMOOTH_POINT_SIZE_GRANULARITY 0x0B13
#define GL_SMOOTH_LINE_WIDTH_RANGE 0x0B22
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY 0x0B23
#define GL_UNSIGNED_BYTE_3_3_2 0x8032
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#define GL_UNSIGNED_INT_10_10_10_2 0x8036
#define GL_RESCALE_NORMAL 0x803A
#define GL_TEXTURE_BINDING_3D 0x806A
#define GL_PACK_SKIP_IMAGES 0x806B
#define GL_PACK_IMAGE_HEIGHT 0x806C
#define GL_UNPACK_SKIP_IMAGES 0x806D
#define GL_UNPACK_IMAGE_HEIGHT 0x806E
#define GL_TEXTURE_3D 0x806F
#define GL_PROXY_TEXTURE_3D 0x8070
#define GL_TEXTURE_DEPTH 0x8071
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_MAX_3D_TEXTURE_SIZE 0x8073
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_MAX_ELEMENTS_VERTICES 0x80E8
#define GL_MAX_ELEMENTS_INDICES 0x80E9
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_LIGHT_MODEL_COLOR_CONTROL 0x81F8
#define GL_SINGLE_COLOR 0x81F9
#define GL_SEPARATE_SPECULAR_COLOR 0x81FA
#define GL_UNSIGNED_BYTE_2_3_3_REV 0x8362
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4_REV 0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_ALIASED_POINT_SIZE_RANGE 0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE 0x846E

typedef void (APIENTRY * PFNGLCOPYTEXSUBIMAGE3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY * PFNGLDRAWRANGEELEMENTS) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (APIENTRY * PFNGLTEXIMAGE3D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY * PFNGLTEXSUBIMAGE3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

extern PFNGLCOPYTEXSUBIMAGE3D glCopyTexSubImage3D;
extern PFNGLDRAWRANGEELEMENTS glDrawRangeElements;
extern PFNGLTEXIMAGE3D glTexImage3D;
extern PFNGLTEXSUBIMAGE3D glTexSubImage3D;

#endif //GL_VERSION_1_2
/* ---------------------------- GL_VERSION_1_3 ---------------------------- */
#ifndef GL_VERSION_1_3
#define GL_VERSION_1_3
#define IC_GL_VERSION_1_3

#define GL_MULTISAMPLE 0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE 0x809F
#define GL_SAMPLE_COVERAGE 0x80A0
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
#define GL_SAMPLE_COVERAGE_VALUE 0x80AA
#define GL_SAMPLE_COVERAGE_INVERT 0x80AB
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7
#define GL_TEXTURE8 0x84C8
#define GL_TEXTURE9 0x84C9
#define GL_TEXTURE10 0x84CA
#define GL_TEXTURE11 0x84CB
#define GL_TEXTURE12 0x84CC
#define GL_TEXTURE13 0x84CD
#define GL_TEXTURE14 0x84CE
#define GL_TEXTURE15 0x84CF
#define GL_TEXTURE16 0x84D0
#define GL_TEXTURE17 0x84D1
#define GL_TEXTURE18 0x84D2
#define GL_TEXTURE19 0x84D3
#define GL_TEXTURE20 0x84D4
#define GL_TEXTURE21 0x84D5
#define GL_TEXTURE22 0x84D6
#define GL_TEXTURE23 0x84D7
#define GL_TEXTURE24 0x84D8
#define GL_TEXTURE25 0x84D9
#define GL_TEXTURE26 0x84DA
#define GL_TEXTURE27 0x84DB
#define GL_TEXTURE28 0x84DC
#define GL_TEXTURE29 0x84DD
#define GL_TEXTURE30 0x84DE
#define GL_TEXTURE31 0x84DF
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE 0x84E1
#define GL_MAX_TEXTURE_UNITS 0x84E2
#define GL_TRANSPOSE_MODELVIEW_MATRIX 0x84E3
#define GL_TRANSPOSE_PROJECTION_MATRIX 0x84E4
#define GL_TRANSPOSE_TEXTURE_MATRIX 0x84E5
#define GL_TRANSPOSE_COLOR_MATRIX 0x84E6
#define GL_SUBTRACT 0x84E7
#define GL_COMPRESSED_ALPHA 0x84E9
#define GL_COMPRESSED_LUMINANCE 0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA 0x84EB
#define GL_COMPRESSED_INTENSITY 0x84EC
#define GL_COMPRESSED_RGB 0x84ED
#define GL_COMPRESSED_RGBA 0x84EE
#define GL_TEXTURE_COMPRESSION_HINT 0x84EF
#define GL_NORMAL_MAP 0x8511
#define GL_REFLECTION_MAP 0x8512
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP 0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE 0x851C
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ADD_SIGNED 0x8574
#define GL_INTERPOLATE 0x8575
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE2_RGB 0x8582
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_SOURCE2_ALPHA 0x858A
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_OPERAND2_ALPHA 0x859A
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE 0x86A0
#define GL_TEXTURE_COMPRESSED 0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS 0x86A3
#define GL_DOT3_RGB 0x86AE
#define GL_DOT3_RGBA 0x86AF
#define GL_MULTISAMPLE_BIT 0x20000000

typedef void (APIENTRY * PFNGLACTIVETEXTURE) (GLenum texture);
typedef void (APIENTRY * PFNGLCLIENTACTIVETEXTURE) (GLenum texture);
typedef void (APIENTRY * PFNGLCOMPRESSEDTEXIMAGE1D) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PFNGLCOMPRESSEDTEXIMAGE2D) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PFNGLCOMPRESSEDTEXIMAGE3D) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE1D) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PFNGLCOMPRESSEDTEXSUBIMAGE3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRY * PFNGLGETCOMPRESSEDTEXIMAGE) (GLenum target, GLint lod, GLvoid *img);
typedef void (APIENTRY * PFNGLLOADTRANSPOSEMATRIXD) (const GLdouble m[16]);
typedef void (APIENTRY * PFNGLLOADTRANSPOSEMATRIXF) (const GLfloat m[16]);
typedef void (APIENTRY * PFNGLMULTTRANSPOSEMATRIXD) (const GLdouble m[16]);
typedef void (APIENTRY * PFNGLMULTTRANSPOSEMATRIXF) (const GLfloat m[16]);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1D) (GLenum target, GLdouble s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1DV) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1F) (GLenum target, GLfloat s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1FV) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1I) (GLenum target, GLint s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1IV) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1S) (GLenum target, GLshort s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1SV) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2D) (GLenum target, GLdouble s, GLdouble t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2DV) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2F) (GLenum target, GLfloat s, GLfloat t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2FV) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2I) (GLenum target, GLint s, GLint t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2IV) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2S) (GLenum target, GLshort s, GLshort t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2SV) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3D) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3DV) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3F) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3FV) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3I) (GLenum target, GLint s, GLint t, GLint r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3IV) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3S) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3SV) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4D) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4DV) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4F) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4FV) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4I) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4IV) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4S) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4SV) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLSAMPLECOVERAGE) (GLclampf value, GLboolean invert);

extern PFNGLACTIVETEXTURE glActiveTexture;
extern PFNGLCLIENTACTIVETEXTURE glClientActiveTexture;
extern PFNGLCOMPRESSEDTEXIMAGE1D glCompressedTexImage1D;
extern PFNGLCOMPRESSEDTEXIMAGE2D glCompressedTexImage2D;
extern PFNGLCOMPRESSEDTEXIMAGE3D glCompressedTexImage3D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE1D glCompressedTexSubImage1D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2D glCompressedTexSubImage2D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3D glCompressedTexSubImage3D;
extern PFNGLGETCOMPRESSEDTEXIMAGE glGetCompressedTexImage;
extern PFNGLLOADTRANSPOSEMATRIXD glLoadTransposeMatrixd;
extern PFNGLLOADTRANSPOSEMATRIXF glLoadTransposeMatrixf;
extern PFNGLMULTTRANSPOSEMATRIXD glMultTransposeMatrixd;
extern PFNGLMULTTRANSPOSEMATRIXF glMultTransposeMatrixf;
extern PFNGLMULTITEXCOORD1D glMultiTexCoord1d;
extern PFNGLMULTITEXCOORD1DV glMultiTexCoord1dv;
extern PFNGLMULTITEXCOORD1F glMultiTexCoord1f;
extern PFNGLMULTITEXCOORD1FV glMultiTexCoord1fv;
extern PFNGLMULTITEXCOORD1I glMultiTexCoord1i;
extern PFNGLMULTITEXCOORD1IV glMultiTexCoord1iv;
extern PFNGLMULTITEXCOORD1S glMultiTexCoord1s;
extern PFNGLMULTITEXCOORD1SV glMultiTexCoord1sv;
extern PFNGLMULTITEXCOORD2D glMultiTexCoord2d;
extern PFNGLMULTITEXCOORD2DV glMultiTexCoord2dv;
extern PFNGLMULTITEXCOORD2F glMultiTexCoord2f;
extern PFNGLMULTITEXCOORD2FV glMultiTexCoord2fv;
extern PFNGLMULTITEXCOORD2I glMultiTexCoord2i;
extern PFNGLMULTITEXCOORD2IV glMultiTexCoord2iv;
extern PFNGLMULTITEXCOORD2S glMultiTexCoord2s;
extern PFNGLMULTITEXCOORD2SV glMultiTexCoord2sv;
extern PFNGLMULTITEXCOORD3D glMultiTexCoord3d;
extern PFNGLMULTITEXCOORD3DV glMultiTexCoord3dv;
extern PFNGLMULTITEXCOORD3F glMultiTexCoord3f;
extern PFNGLMULTITEXCOORD3FV glMultiTexCoord3fv;
extern PFNGLMULTITEXCOORD3I glMultiTexCoord3i;
extern PFNGLMULTITEXCOORD3IV glMultiTexCoord3iv;
extern PFNGLMULTITEXCOORD3S glMultiTexCoord3s;
extern PFNGLMULTITEXCOORD3SV glMultiTexCoord3sv;
extern PFNGLMULTITEXCOORD4D glMultiTexCoord4d;
extern PFNGLMULTITEXCOORD4DV glMultiTexCoord4dv;
extern PFNGLMULTITEXCOORD4F glMultiTexCoord4f;
extern PFNGLMULTITEXCOORD4FV glMultiTexCoord4fv;
extern PFNGLMULTITEXCOORD4I glMultiTexCoord4i;
extern PFNGLMULTITEXCOORD4IV glMultiTexCoord4iv;
extern PFNGLMULTITEXCOORD4S glMultiTexCoord4s;
extern PFNGLMULTITEXCOORD4SV glMultiTexCoord4sv;
extern PFNGLSAMPLECOVERAGE glSampleCoverage;

#endif //GL_VERSION_1_3
/* ----------------------------- GL_VERSION_1_4 ---------------------------- */
#ifndef GL_VERSION_1_4
#define GL_VERSION_1_4
#define IC_GL_VERSION_1_4

#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_POINT_SIZE_MIN 0x8126
#define GL_POINT_SIZE_MAX 0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE 0x8128
#define GL_POINT_DISTANCE_ATTENUATION 0x8129
#define GL_GENERATE_MIPMAP 0x8191
#define GL_GENERATE_MIPMAP_HINT 0x8192
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_MIRRORED_REPEAT 0x8370
#define GL_FOG_COORDINATE_SOURCE 0x8450
#define GL_FOG_COORDINATE 0x8451
#define GL_FRAGMENT_DEPTH 0x8452
#define GL_CURRENT_FOG_COORDINATE 0x8453
#define GL_FOG_COORDINATE_ARRAY_TYPE 0x8454
#define GL_FOG_COORDINATE_ARRAY_STRIDE 0x8455
#define GL_FOG_COORDINATE_ARRAY_POINTER 0x8456
#define GL_FOG_COORDINATE_ARRAY 0x8457
#define GL_COLOR_SUM 0x8458
#define GL_CURRENT_SECONDARY_COLOR 0x8459
#define GL_SECONDARY_COLOR_ARRAY_SIZE 0x845A
#define GL_SECONDARY_COLOR_ARRAY_TYPE 0x845B
#define GL_SECONDARY_COLOR_ARRAY_STRIDE 0x845C
#define GL_SECONDARY_COLOR_ARRAY_POINTER 0x845D
#define GL_SECONDARY_COLOR_ARRAY 0x845E
#define GL_MAX_TEXTURE_LOD_BIAS 0x84FD
#define GL_TEXTURE_FILTER_CONTROL 0x8500
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_TEXTURE_DEPTH_SIZE 0x884A
#define GL_DEPTH_TEXTURE_MODE 0x884B
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_R_TO_TEXTURE 0x884E

typedef void (APIENTRY * PFNGLBLENDCOLOR) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (APIENTRY * PFNGLBLENDEQUATION) (GLenum mode);
typedef void (APIENTRY * PFNGLBLENDFUNCSEPARATE) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (APIENTRY * PFNGLFOGCOORDPOINTER) (GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLFOGCOORDD) (GLdouble coord);
typedef void (APIENTRY * PFNGLFOGCOORDDV) (const GLdouble *coord);
typedef void (APIENTRY * PFNGLFOGCOORDF) (GLfloat coord);
typedef void (APIENTRY * PFNGLFOGCOORDFV) (const GLfloat *coord);
typedef void (APIENTRY * PFNGLMULTIDRAWARRAYS) (GLenum mode, GLint *first, GLsizei *count, GLsizei primcount);
typedef void (APIENTRY * PFNGLMULTIDRAWELEMENTS) (GLenum mode, GLsizei *count, GLenum type, const GLvoid **indices, GLsizei primcount);
typedef void (APIENTRY * PFNGLPOINTPARAMETERF) (GLenum pname, GLfloat param);
typedef void (APIENTRY * PFNGLPOINTPARAMETERFV) (GLenum pname, GLfloat *params);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3B) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3BV) (const GLbyte *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3D) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3DV) (const GLdouble *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3F) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3FV) (const GLfloat *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3I) (GLint red, GLint green, GLint blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3IV) (const GLint *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3S) (GLshort red, GLshort green, GLshort blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3SV) (const GLshort *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3UB) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3UBV) (const GLubyte *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3UI) (GLuint red, GLuint green, GLuint blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3UIV) (const GLuint *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3US) (GLushort red, GLushort green, GLushort blue);
typedef void (APIENTRY * PFNGLSECONDARYCOLOR3USV) (const GLushort *v);
typedef void (APIENTRY * PFNGLSECONDARYCOLORPOINTER) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (APIENTRY * PFNGLWINDOWPOS2D) (GLdouble x, GLdouble y);
typedef void (APIENTRY * PFNGLWINDOWPOS2DV) (const GLdouble *p);
typedef void (APIENTRY * PFNGLWINDOWPOS2F) (GLfloat x, GLfloat y);
typedef void (APIENTRY * PFNGLWINDOWPOS2FV) (const GLfloat *p);
typedef void (APIENTRY * PFNGLWINDOWPOS2I) (GLint x, GLint y);
typedef void (APIENTRY * PFNGLWINDOWPOS2IV) (const GLint *p);
typedef void (APIENTRY * PFNGLWINDOWPOS2S) (GLshort x, GLshort y);
typedef void (APIENTRY * PFNGLWINDOWPOS2SV) (const GLshort *p);
typedef void (APIENTRY * PFNGLWINDOWPOS3D) (GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLWINDOWPOS3DV) (const GLdouble *p);
typedef void (APIENTRY * PFNGLWINDOWPOS3F) (GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLWINDOWPOS3FV) (const GLfloat *p);
typedef void (APIENTRY * PFNGLWINDOWPOS3I) (GLint x, GLint y, GLint z);
typedef void (APIENTRY * PFNGLWINDOWPOS3IV) (const GLint *p);
typedef void (APIENTRY * PFNGLWINDOWPOS3S) (GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY * PFNGLWINDOWPOS3SV) (const GLshort *p);

extern PFNGLBLENDCOLOR glBlendColor;
extern PFNGLBLENDEQUATION glBlendEquation;
extern PFNGLBLENDFUNCSEPARATE glBlendFuncSeparate;
extern PFNGLFOGCOORDPOINTER glFogCoordPointer;
extern PFNGLFOGCOORDD glFogCoordd;
extern PFNGLFOGCOORDDV glFogCoorddv;
extern PFNGLFOGCOORDF glFogCoordf;
extern PFNGLFOGCOORDFV glFogCoordfv;
extern PFNGLMULTIDRAWARRAYS glMultiDrawArrays;
extern PFNGLMULTIDRAWELEMENTS glMultiDrawElements;
extern PFNGLPOINTPARAMETERF glPointParameterf;
extern PFNGLPOINTPARAMETERFV glPointParameterfv;
extern PFNGLSECONDARYCOLOR3B glSecondaryColor3b;
extern PFNGLSECONDARYCOLOR3BV glSecondaryColor3bv;
extern PFNGLSECONDARYCOLOR3D glSecondaryColor3d;
extern PFNGLSECONDARYCOLOR3DV glSecondaryColor3dv;
extern PFNGLSECONDARYCOLOR3F glSecondaryColor3f;
extern PFNGLSECONDARYCOLOR3FV glSecondaryColor3fv;
extern PFNGLSECONDARYCOLOR3I glSecondaryColor3i;
extern PFNGLSECONDARYCOLOR3IV glSecondaryColor3iv;
extern PFNGLSECONDARYCOLOR3S glSecondaryColor3s;
extern PFNGLSECONDARYCOLOR3SV glSecondaryColor3sv;
extern PFNGLSECONDARYCOLOR3UB glSecondaryColor3ub;
extern PFNGLSECONDARYCOLOR3UBV glSecondaryColor3ubv;
extern PFNGLSECONDARYCOLOR3UI glSecondaryColor3ui;
extern PFNGLSECONDARYCOLOR3UIV glSecondaryColor3uiv;
extern PFNGLSECONDARYCOLOR3US glSecondaryColor3us;
extern PFNGLSECONDARYCOLOR3USV glSecondaryColor3usv;
extern PFNGLSECONDARYCOLORPOINTER glSecondaryColorPointer;
extern PFNGLWINDOWPOS2D glWindowPos2d;
extern PFNGLWINDOWPOS2DV glWindowPos2dv;
extern PFNGLWINDOWPOS2F glWindowPos2f;
extern PFNGLWINDOWPOS2FV glWindowPos2fv;
extern PFNGLWINDOWPOS2I glWindowPos2i;
extern PFNGLWINDOWPOS2IV glWindowPos2iv;
extern PFNGLWINDOWPOS2S glWindowPos2s;
extern PFNGLWINDOWPOS2SV glWindowPos2sv;
extern PFNGLWINDOWPOS3D glWindowPos3d;
extern PFNGLWINDOWPOS3DV glWindowPos3dv;
extern PFNGLWINDOWPOS3F glWindowPos3f;
extern PFNGLWINDOWPOS3FV glWindowPos3fv;
extern PFNGLWINDOWPOS3I glWindowPos3i;
extern PFNGLWINDOWPOS3IV glWindowPos3iv;
extern PFNGLWINDOWPOS3S glWindowPos3s;
extern PFNGLWINDOWPOS3SV glWindowPos3sv;

#endif //GL_VERSION_1_4
/* ----------------------------- GL_VERSION_1_5 ---------------------------- */
#ifndef GL_VERSION_1_5
#define GL_VERSION_1_5

#define GL_FOG_COORD_SRC GL_FOG_COORDINATE_SOURCE
#define GL_FOG_COORD GL_FOG_COORDINATE
#define GL_FOG_COORD_ARRAY GL_FOG_COORDINATE_ARRAY
#define GL_SRC0_RGB GL_SOURCE0_RGB
#define GL_FOG_COORD_ARRAY_POINTER GL_FOG_COORDINATE_ARRAY_POINTER
#define GL_FOG_COORD_ARRAY_TYPE GL_FOG_COORDINATE_ARRAY_TYPE
#define GL_SRC1_ALPHA GL_SOURCE1_ALPHA
#define GL_CURRENT_FOG_COORD GL_CURRENT_FOG_COORDINATE
#define GL_FOG_COORD_ARRAY_STRIDE GL_FOG_COORDINATE_ARRAY_STRIDE
#define GL_SRC0_ALPHA GL_SOURCE0_ALPHA
#define GL_SRC1_RGB GL_SOURCE1_RGB
#define GL_FOG_COORD_ARRAY_BUFFER_BINDING GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING
#define GL_SRC2_ALPHA GL_SOURCE2_ALPHA
#define GL_SRC2_RGB GL_SOURCE2_RGB
#define GL_BUFFER_SIZE 0x8764
#define GL_BUFFER_USAGE 0x8765
#define GL_QUERY_COUNTER_BITS 0x8864
#define GL_CURRENT_QUERY 0x8865
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_BUFFER_ACCESS 0x88BB
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA
#define GL_SAMPLES_PASSED 0x8914

typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

typedef void (APIENTRY * PFNGLBEGINQUERY) (GLenum target, GLuint id);
typedef void (APIENTRY * PFNGLBINDBUFFER) (GLenum target, GLuint buffer);
typedef void (APIENTRY * PFNGLBUFFERDATA) (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
typedef void (APIENTRY * PFNGLBUFFERSUBDATA) (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
typedef void (APIENTRY * PFNGLDELETEBUFFERS) (GLsizei n, const GLuint* buffers);
typedef void (APIENTRY * PFNGLDELETEQUERIES) (GLsizei n, const GLuint* ids);
typedef void (APIENTRY * PFNGLENDQUERY) (GLenum target);
typedef void (APIENTRY * PFNGLGENBUFFERS) (GLsizei n, GLuint* buffers);
typedef void (APIENTRY * PFNGLGENQUERIES) (GLsizei n, GLuint* ids);
typedef void (APIENTRY * PFNGLGETBUFFERPARAMETERIV) (GLenum target, GLenum pname, GLint* params);
typedef void (APIENTRY * PFNGLGETBUFFERPOINTERV) (GLenum target, GLenum pname, GLvoid** params);
typedef void (APIENTRY * PFNGLGETBUFFERSUBDATA) (GLenum target, GLintptr offset, GLsizeiptr size, GLvoid* data);
typedef void (APIENTRY * PFNGLGETQUERYOBJECTIV) (GLuint id, GLenum pname, GLint* params);
typedef void (APIENTRY * PFNGLGETQUERYOBJECTUIV) (GLuint id, GLenum pname, GLuint* params);
typedef void (APIENTRY * PFNGLGETQUERYIV) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (APIENTRY * PFNGLISBUFFER) (GLuint buffer);
typedef GLboolean (APIENTRY * PFNGLISQUERY) (GLuint id);
typedef GLvoid* (APIENTRY * PFNGLMAPBUFFER) (GLenum target, GLenum access);
typedef GLboolean (APIENTRY * PFNGLUNMAPBUFFER) (GLenum target);

extern PFNGLBEGINQUERY glBeginQuery;
extern PFNGLBINDBUFFER glBindBuffer;
extern PFNGLBUFFERDATA glBufferData;
extern PFNGLBUFFERSUBDATA glBufferSubData;
extern PFNGLDELETEBUFFERS glDeleteBuffers;
extern PFNGLDELETEQUERIES glDeleteQueries;
extern PFNGLENDQUERY glEndQuery;
extern PFNGLGENBUFFERS glGenBuffers;
extern PFNGLGENQUERIES glGenQueries;
extern PFNGLGETBUFFERPARAMETERIV glGetBufferParameteriv;
extern PFNGLGETBUFFERPOINTERV glGetBufferPointerv;
extern PFNGLGETBUFFERSUBDATA glGetBufferSubData;
extern PFNGLGETQUERYOBJECTIV glGetQueryObjectiv;
extern PFNGLGETQUERYOBJECTUIV glGetQueryObjectuiv;
extern PFNGLGETQUERYIV glGetQueryiv;
extern PFNGLISBUFFER glIsBuffer;
extern PFNGLISQUERY glIsQuery;
extern PFNGLMAPBUFFER glMapBuffer;
extern PFNGLUNMAPBUFFER glUnmapBuffer;

#endif //GL_VERSION_1_5
/* ---------------------------- GL_VERSION_2_0 ---------------------------- */
#ifndef GL_VERSION_2_0
#define GL_VERSION_2_0
#define IC_GL_VERSION_2_0

#define GL_BLEND_EQUATION_RGB GL_BLEND_EQUATION
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_CURRENT_VERTEX_ATTRIB 0x8626
#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE 0x8643
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_STENCIL_BACK_FUNC 0x8800
#define GL_STENCIL_BACK_FAIL 0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS 0x8803
#define GL_MAX_DRAW_BUFFERS 0x8824
#define GL_DRAW_BUFFER0 0x8825
#define GL_DRAW_BUFFER1 0x8826
#define GL_DRAW_BUFFER2 0x8827
#define GL_DRAW_BUFFER3 0x8828
#define GL_DRAW_BUFFER4 0x8829
#define GL_DRAW_BUFFER5 0x882A
#define GL_DRAW_BUFFER6 0x882B
#define GL_DRAW_BUFFER7 0x882C
#define GL_DRAW_BUFFER8 0x882D
#define GL_DRAW_BUFFER9 0x882E
#define GL_DRAW_BUFFER10 0x882F
#define GL_DRAW_BUFFER11 0x8830
#define GL_DRAW_BUFFER12 0x8831
#define GL_DRAW_BUFFER13 0x8832
#define GL_DRAW_BUFFER14 0x8833
#define GL_DRAW_BUFFER15 0x8834
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_POINT_SPRITE 0x8861
#define GL_COORD_REPLACE 0x8862
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GL_MAX_TEXTURE_COORDS 0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#define GL_MAX_VARYING_FLOATS 0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_SHADER_TYPE 0x8B4F
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_SAMPLER_1D 0x8B5D
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_3D 0x8B5F
#define GL_SAMPLER_CUBE 0x8B60
#define GL_SAMPLER_1D_SHADOW 0x8B61
#define GL_SAMPLER_2D_SHADOW 0x8B62
#define GL_DELETE_STATUS 0x8B80
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH 0x8B8A
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT 0x8B8B
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
#define GL_LOWER_LEFT 0x8CA1
#define GL_UPPER_LEFT 0x8CA2
#define GL_STENCIL_BACK_REF 0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK 0x8CA4
#define GL_STENCIL_BACK_WRITEMASK 0x8CA5

typedef char GLchar;

typedef void (APIENTRY * PFNGLATTACHSHADER) (GLuint program, GLuint shader);
typedef void (APIENTRY * PFNGLBINDATTRIBLOCATION) (GLuint program, GLuint index, const GLchar* name);
typedef void (APIENTRY * PFNGLBLENDEQUATIONSEPARATE) (GLenum, GLenum);
typedef void (APIENTRY * PFNGLCOMPILESHADER) (GLuint shader);
typedef GLuint (APIENTRY * PFNGLCREATEPROGRAM) (void);
typedef GLuint (APIENTRY * PFNGLCREATESHADER) (GLenum type);
typedef void (APIENTRY * PFNGLDELETEPROGRAM) (GLuint program);
typedef void (APIENTRY * PFNGLDELETESHADER) (GLuint shader);
typedef void (APIENTRY * PFNGLDETACHSHADER) (GLuint program, GLuint shader);
typedef void (APIENTRY * PFNGLDISABLEVERTEXATTRIBARRAY) (GLuint);
typedef void (APIENTRY * PFNGLDRAWBUFFERS) (GLsizei n, const GLenum* bufs);
typedef void (APIENTRY * PFNGLENABLEVERTEXATTRIBARRAY) (GLuint);
typedef void (APIENTRY * PFNGLGETACTIVEATTRIB) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void (APIENTRY * PFNGLGETACTIVEUNIFORM) (GLuint program, GLuint index, GLsizei maxLength, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
typedef void (APIENTRY * PFNGLGETATTACHEDSHADERS) (GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
typedef GLint (APIENTRY * PFNGLGETATTRIBLOCATION) (GLuint program, const GLchar* name);
typedef void (APIENTRY * PFNGLGETPROGRAMINFOLOG) (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRY * PFNGLGETPROGRAMIV) (GLuint program, GLenum pname, GLint* param);
typedef void (APIENTRY * PFNGLGETSHADERINFOLOG) (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRY * PFNGLGETSHADERSOURCE) (GLint obj, GLsizei maxLength, GLsizei* length, GLchar* source);
typedef void (APIENTRY * PFNGLGETSHADERIV) (GLuint shader, GLenum pname, GLint* param);
typedef GLint (APIENTRY * PFNGLGETUNIFORMLOCATION) (GLint programObj, const GLchar* name);
typedef void (APIENTRY * PFNGLGETUNIFORMFV) (GLuint program, GLint location, GLfloat* params);
typedef void (APIENTRY * PFNGLGETUNIFORMIV) (GLuint program, GLint location, GLint* params);
typedef void (APIENTRY * PFNGLGETVERTEXATTRIBPOINTERV) (GLuint, GLenum, GLvoid*);
typedef void (APIENTRY * PFNGLGETVERTEXATTRIBDV) (GLuint, GLenum, GLdouble*);
typedef void (APIENTRY * PFNGLGETVERTEXATTRIBFV) (GLuint, GLenum, GLfloat*);
typedef void (APIENTRY * PFNGLGETVERTEXATTRIBIV) (GLuint, GLenum, GLint*);
typedef GLboolean (APIENTRY * PFNGLISPROGRAM) (GLuint program);
typedef GLboolean (APIENTRY * PFNGLISSHADER) (GLuint shader);
typedef void (APIENTRY * PFNGLLINKPROGRAM) (GLuint program);
typedef void (APIENTRY * PFNGLSHADERSOURCE) (GLuint shader, GLsizei count, const GLchar** strings, const GLint* lengths);
typedef void (APIENTRY * PFNGLSTENCILFUNCSEPARATE) (GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
typedef void (APIENTRY * PFNGLSTENCILMASKSEPARATE) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLSTENCILOPSEPARATE) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (APIENTRY * PFNGLUNIFORM1F) (GLint location, GLfloat v0);
typedef void (APIENTRY * PFNGLUNIFORM1FV) (GLint location, GLsizei count, const GLfloat* value);
typedef void (APIENTRY * PFNGLUNIFORM1I) (GLint location, GLint v0);
typedef void (APIENTRY * PFNGLUNIFORM1IV) (GLint location, GLsizei count, const GLint* value);
typedef void (APIENTRY * PFNGLUNIFORM2F) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY * PFNGLUNIFORM2FV) (GLint location, GLsizei count, const GLfloat* value);
typedef void (APIENTRY * PFNGLUNIFORM2I) (GLint location, GLint v0, GLint v1);
typedef void (APIENTRY * PFNGLUNIFORM2IV) (GLint location, GLsizei count, const GLint* value);
typedef void (APIENTRY * PFNGLUNIFORM3F) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY * PFNGLUNIFORM3FV) (GLint location, GLsizei count, const GLfloat* value);
typedef void (APIENTRY * PFNGLUNIFORM3I) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRY * PFNGLUNIFORM3IV) (GLint location, GLsizei count, const GLint* value);
typedef void (APIENTRY * PFNGLUNIFORM4F) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY * PFNGLUNIFORM4FV) (GLint location, GLsizei count, const GLfloat* value);
typedef void (APIENTRY * PFNGLUNIFORM4I) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRY * PFNGLUNIFORM4IV) (GLint location, GLsizei count, const GLint* value);
typedef void (APIENTRY * PFNGLUNIFORMMATRIX2FV) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (APIENTRY * PFNGLUNIFORMMATRIX3FV) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (APIENTRY * PFNGLUNIFORMMATRIX4FV) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (APIENTRY * PFNGLUSEPROGRAM) (GLuint program);
typedef void (APIENTRY * PFNGLVALIDATEPROGRAM) (GLuint program);
typedef void (APIENTRY * PFNGLVERTEXATTRIB1D) (GLuint index, GLdouble x);
typedef void (APIENTRY * PFNGLVERTEXATTRIB1DV) (GLuint index, const GLdouble* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB1F) (GLuint index, GLfloat x);
typedef void (APIENTRY * PFNGLVERTEXATTRIB1FV) (GLuint index, const GLfloat* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB1S) (GLuint index, GLshort x);
typedef void (APIENTRY * PFNGLVERTEXATTRIB1SV) (GLuint index, const GLshort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB2D) (GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRY * PFNGLVERTEXATTRIB2DV) (GLuint index, const GLdouble* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB2F) (GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRY * PFNGLVERTEXATTRIB2FV) (GLuint index, const GLfloat* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB2S) (GLuint index, GLshort x, GLshort y);
typedef void (APIENTRY * PFNGLVERTEXATTRIB2SV) (GLuint index, const GLshort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB3D) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY * PFNGLVERTEXATTRIB3DV) (GLuint index, const GLdouble* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB3F) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * PFNGLVERTEXATTRIB3FV) (GLuint index, const GLfloat* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB3S) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY * PFNGLVERTEXATTRIB3SV) (GLuint index, const GLshort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NBV) (GLuint index, const GLbyte* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NIV) (GLuint index, const GLint* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NSV) (GLuint index, const GLshort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NUB) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NUBV) (GLuint index, const GLubyte* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NUIV) (GLuint index, const GLuint* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4NUSV) (GLuint index, const GLushort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4BV) (GLuint index, const GLbyte* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4D) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4DV) (GLuint index, const GLdouble* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4F) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4FV) (GLuint index, const GLfloat* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4IV) (GLuint index, const GLint* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4S) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4SV) (GLuint index, const GLshort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4UBV) (GLuint index, const GLubyte* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4UIV) (GLuint index, const GLuint* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIB4USV) (GLuint index, const GLushort* v);
typedef void (APIENTRY * PFNGLVERTEXATTRIBPOINTER) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);

extern PFNGLATTACHSHADER glAttachShader;
extern PFNGLBINDATTRIBLOCATION glBindAttribLocation;
extern PFNGLBLENDEQUATIONSEPARATE glBlendEquationSeparate;
extern PFNGLCOMPILESHADER glCompileShader;
extern PFNGLCREATEPROGRAM glCreateProgram;
extern PFNGLCREATESHADER glCreateShader;
extern PFNGLDELETEPROGRAM glDeleteProgram;
extern PFNGLDELETESHADER glDeleteShader;
extern PFNGLDETACHSHADER glDetachShader;
extern PFNGLDISABLEVERTEXATTRIBARRAY glDisableVertexAttribArray;
extern PFNGLDRAWBUFFERS glDrawBuffers;
extern PFNGLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray;
extern PFNGLGETACTIVEATTRIB glGetActiveAttrib;
extern PFNGLGETACTIVEUNIFORM glGetActiveUniform;
extern PFNGLGETATTACHEDSHADERS glGetAttachedShaders;
extern PFNGLGETATTRIBLOCATION glGetAttribLocation;
extern PFNGLGETPROGRAMINFOLOG glGetProgramInfoLog;
extern PFNGLGETPROGRAMIV glGetProgramiv;
extern PFNGLGETSHADERINFOLOG glGetShaderInfoLog;
extern PFNGLGETSHADERSOURCE glGetShaderSource;
extern PFNGLGETSHADERIV glGetShaderiv;
extern PFNGLGETUNIFORMLOCATION glGetUniformLocation;
extern PFNGLGETUNIFORMFV glGetUniformfv;
extern PFNGLGETUNIFORMIV glGetUniformiv;
extern PFNGLGETVERTEXATTRIBPOINTERV glGetVertexAttribPointerv;
extern PFNGLGETVERTEXATTRIBDV glGetVertexAttribdv;
extern PFNGLGETVERTEXATTRIBFV glGetVertexAttribfv;
extern PFNGLGETVERTEXATTRIBIV glGetVertexAttribiv;
extern PFNGLISPROGRAM glIsProgram;
extern PFNGLISSHADER glIsShader;
extern PFNGLLINKPROGRAM glLinkProgram;
extern PFNGLSHADERSOURCE glShaderSource;
extern PFNGLSTENCILFUNCSEPARATE glStencilFuncSeparate;
extern PFNGLSTENCILMASKSEPARATE glStencilMaskSeparate;
extern PFNGLSTENCILOPSEPARATE glStencilOpSeparate;
extern PFNGLUNIFORM1F glUniform1f;
extern PFNGLUNIFORM1FV glUniform1fv;
extern PFNGLUNIFORM1I glUniform1i;
extern PFNGLUNIFORM1IV glUniform1iv;
extern PFNGLUNIFORM2F glUniform2f;
extern PFNGLUNIFORM2FV glUniform2fv;
extern PFNGLUNIFORM2I glUniform2i;
extern PFNGLUNIFORM2IV glUniform2iv;
extern PFNGLUNIFORM3F glUniform3f;
extern PFNGLUNIFORM3FV glUniform3fv;
extern PFNGLUNIFORM3I glUniform3i;
extern PFNGLUNIFORM3IV glUniform3iv;
extern PFNGLUNIFORM4F glUniform4f;
extern PFNGLUNIFORM4FV glUniform4fv;
extern PFNGLUNIFORM4I glUniform4i;
extern PFNGLUNIFORM4IV glUniform4iv;
extern PFNGLUNIFORMMATRIX2FV glUniformMatrix2fv;
extern PFNGLUNIFORMMATRIX3FV glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX4FV glUniformMatrix4fv;
extern PFNGLUSEPROGRAM glUseProgram;
extern PFNGLVALIDATEPROGRAM glValidateProgram;
extern PFNGLVERTEXATTRIB1D glVertexAttrib1d;
extern PFNGLVERTEXATTRIB1DV glVertexAttrib1dv;
extern PFNGLVERTEXATTRIB1F glVertexAttrib1f;
extern PFNGLVERTEXATTRIB1FV glVertexAttrib1fv;
extern PFNGLVERTEXATTRIB1S glVertexAttrib1s;
extern PFNGLVERTEXATTRIB1SV glVertexAttrib1sv;
extern PFNGLVERTEXATTRIB2D glVertexAttrib2d;
extern PFNGLVERTEXATTRIB2DV glVertexAttrib2dv;
extern PFNGLVERTEXATTRIB2F glVertexAttrib2f;
extern PFNGLVERTEXATTRIB2FV glVertexAttrib2fv;
extern PFNGLVERTEXATTRIB2S glVertexAttrib2s;
extern PFNGLVERTEXATTRIB2SV glVertexAttrib2sv;
extern PFNGLVERTEXATTRIB3D glVertexAttrib3d;
extern PFNGLVERTEXATTRIB3DV glVertexAttrib3dv;
extern PFNGLVERTEXATTRIB3F glVertexAttrib3f;
extern PFNGLVERTEXATTRIB3FV glVertexAttrib3fv;
extern PFNGLVERTEXATTRIB3S glVertexAttrib3s;
extern PFNGLVERTEXATTRIB3SV glVertexAttrib3sv;
extern PFNGLVERTEXATTRIB4NBV glVertexAttrib4Nbv;
extern PFNGLVERTEXATTRIB4NIV glVertexAttrib4Niv;
extern PFNGLVERTEXATTRIB4NSV glVertexAttrib4Nsv;
extern PFNGLVERTEXATTRIB4NUB glVertexAttrib4Nub;
extern PFNGLVERTEXATTRIB4NUBV glVertexAttrib4Nubv;
extern PFNGLVERTEXATTRIB4NUIV glVertexAttrib4Nuiv;
extern PFNGLVERTEXATTRIB4NUSV glVertexAttrib4Nusv;
extern PFNGLVERTEXATTRIB4BV glVertexAttrib4bv;
extern PFNGLVERTEXATTRIB4D glVertexAttrib4d;
extern PFNGLVERTEXATTRIB4DV glVertexAttrib4dv;
extern PFNGLVERTEXATTRIB4F glVertexAttrib4f;
extern PFNGLVERTEXATTRIB4FV glVertexAttrib4fv;
extern PFNGLVERTEXATTRIB4IV glVertexAttrib4iv;
extern PFNGLVERTEXATTRIB4S glVertexAttrib4s;
extern PFNGLVERTEXATTRIB4SV glVertexAttrib4sv;
extern PFNGLVERTEXATTRIB4UBV glVertexAttrib4ubv;
extern PFNGLVERTEXATTRIB4UIV glVertexAttrib4uiv;
extern PFNGLVERTEXATTRIB4USV glVertexAttrib4usv;
extern PFNGLVERTEXATTRIBPOINTER glVertexAttribPointer;

#endif //GL_VERSION_2_0
