#version 120
varying vec3 pos,normal,tangent;
varying float signT;
varying vec2 uv0,uv1;
varying vec4 color;
uniform float mirrorSign;
void main(){
 vec4 eye=gl_ModelViewMatrix*gl_Vertex;
 pos=eye.xyz;normal=normalize(gl_NormalMatrix*gl_Normal);
 tangent=mat3(gl_ModelViewMatrix)*gl_MultiTexCoord2.xyz;
 signT=gl_MultiTexCoord2.w*mirrorSign;
 uv0=gl_MultiTexCoord0.xy;uv1=gl_MultiTexCoord1.xy;color=gl_Color;
 gl_Position=gl_ProjectionMatrix*eye;
}
