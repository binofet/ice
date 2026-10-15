#version 120
#ifdef ICE_SKINNED
uniform mat4 joints[64];
#endif
varying vec3 pos,normal,tangent;
varying float signT;
varying vec2 uv0,uv1;
varying vec4 color;
uniform float mirrorSign;
void main(){
 vec4 vertex=gl_Vertex;
 vec3 n=gl_Normal,t=gl_MultiTexCoord2.xyz;
 float handedness=mirrorSign;
#ifdef ICE_SKINNED
 mat4 skin=gl_MultiTexCoord4.x*joints[int(gl_MultiTexCoord3.x)]
          +gl_MultiTexCoord4.y*joints[int(gl_MultiTexCoord3.y)]
          +gl_MultiTexCoord4.z*joints[int(gl_MultiTexCoord3.z)]
          +gl_MultiTexCoord4.w*joints[int(gl_MultiTexCoord3.w)];
 vertex=skin*vertex;
 mat3 linear=mat3(skin);
 vec3 a=linear[0],b=linear[1],c=linear[2];
 float determinant=dot(a,cross(b,c));
 handedness=determinant<0.0?-1.0:1.0;
 // Cofactor / determinant gives inverse-transpose; normalization removes magnitude.
 mat3 cofactors=mat3(cross(b,c),cross(c,a),cross(a,b));
 vec3 transformed=cofactors*n*handedness;
 n=dot(transformed,transformed)>1e-16?normalize(transformed):n;
 t=linear*t;
#endif
 vec4 eye=gl_ModelViewMatrix*vertex;
 pos=eye.xyz;normal=normalize(gl_NormalMatrix*n);
 tangent=mat3(gl_ModelViewMatrix)*t;
 signT=gl_MultiTexCoord2.w*handedness;
 uv0=gl_MultiTexCoord0.xy;uv1=gl_MultiTexCoord1.xy;color=gl_Color;
 gl_Position=gl_ProjectionMatrix*eye;
}
