#version 120
varying vec3 pos,normal,tangent;
varying float signT;
varying vec2 uv0,uv1;
varying vec4 color;
uniform sampler2D baseMap,mrMap,normalMap,aoMap,emissiveMap;
uniform int baseUV,mrUV,normalUV,aoUV,emissiveUV;
uniform vec4 baseFactor;
uniform vec3 emissiveFactor;
uniform float metallic,roughness,normalScale,aoStrength,alphaCutoff,exposure;
uniform int alphaMode,unlit,debugMode;
const float PI=3.14159265359;
vec2 texcoord(int index){return index==1?uv1:uv0;}
vec3 linearize(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
vec3 encode(vec3 c){return mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-.055,step(vec3(.0031308),c));}
vec3 fresnel(float h,vec3 f0){return f0+(1.0-f0)*pow(1.0-h,5.0);}
vec3 light(vec3 N,vec3 V,vec3 L,vec3 radiance,vec3 albedo,vec3 f0,float metal,float rough){
 vec3 H=normalize(V+L);float nl=max(dot(N,L),0.0),nv=max(dot(N,V),.001),nh=max(dot(N,H),0.0),vh=max(dot(V,H),0.0);
 float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.0)+1.0,D=a2/(PI*d*d);
 float k=(rough+1.0)*(rough+1.0)/8.0;
 float G=(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));
 vec3 F=fresnel(vh,f0),spec=D*G*F/max(4.0*nv*nl,.001);
 return ((1.0-F)*(1.0-metal)*albedo/PI+spec)*radiance*nl;
}
void main(){
 vec4 base=texture2D(baseMap,texcoord(baseUV));vec3 albedo=linearize(base.rgb)*baseFactor.rgb*color.rgb;
 float alpha=base.a*baseFactor.a*color.a;
 if(alphaMode==1&&alpha<alphaCutoff)discard;
 vec3 mr=texture2D(mrMap,texcoord(mrUV)).rgb;
 float metal=clamp(metallic*mr.b,0.0,1.0),rough=clamp(roughness*mr.g,.045,1.0);
 vec3 N=normalize(normal);vec3 T=normalize(tangent-N*dot(N,tangent));vec3 B=cross(N,T)*signT;
 vec3 mapped=texture2D(normalMap,texcoord(normalUV)).xyz*2.0-1.0;mapped.xy*=normalScale;
 N=normalize(mat3(T,B,N)*normalize(mapped));if(!gl_FrontFacing)N=-N;
 vec3 V=normalize(-pos),f0=mix(vec3(.04),albedo,metal);
 float ao=mix(1.0,texture2D(aoMap,texcoord(aoUV)).r,aoStrength);
 vec3 emission=linearize(texture2D(emissiveMap,texcoord(emissiveUV)).rgb)*emissiveFactor;
 vec3 c=light(N,V,normalize(vec3(-.5,.8,1.0)),vec3(3.5,3.2,2.8),albedo,f0,metal,rough);
 c+=light(N,V,normalize(vec3(.8,.2,.5)),vec3(.7,.95,1.5),albedo,f0,metal,rough);
 // Analytic studio environment, deliberately separate from direct-light GGX.
 vec3 R=reflect(-V,N);vec3 environment=mix(vec3(.07,.08,.1),vec3(.5,.65,.85),smoothstep(-.4,.8,R.y));
 float strip=pow(max(0.0,dot(R,normalize(vec3(-.3,.6,1.0)))),mix(100.0,2.0,rough));environment+=vec3(2.0,1.8,1.4)*strip*(1.0-rough*.65);
 vec3 F=fresnel(max(dot(N,V),0.0),f0);
 c+=(albedo*(1.0-metal)*.16+environment*F*(1.0-.5*rough))*ao;
 c+=emission;if(unlit==1)c=albedo;
 c*=exposure;
 if(debugMode==1)c=albedo;if(debugMode==2)c=N*.5+.5;
 if(debugMode==3)c=vec3(metal,rough,0);if(debugMode==4)c=vec3(fract(uv0),0);
 gl_FragColor=vec4(clamp(c,vec3(0.0),vec3(60000.0)),alphaMode==2?alpha:1.0);
}
