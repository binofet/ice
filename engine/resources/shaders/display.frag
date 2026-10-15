#version 120
varying vec2 uv;uniform sampler2D image;uniform int debugMode;
vec3 encode(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-.055,step(vec3(.0031308),c));}
void main(){vec3 c=texture2D(image,uv).rgb;if(debugMode==0)c=c/(vec3(1)+c);if(debugMode<=1)c=encode(c);gl_FragColor=vec4(c,1.0);}
