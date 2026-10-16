layout(std140) uniform parameters { vec4 settings; };
#ifdef ICE_VERTEX_STAGE
out vec2 uv;
void fullscreen() {
	const vec2 corners[3] = vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
	vec2 p = corners[gl_VertexID];
	gl_Position = vec4(p,0,1);
	uv = vec2(p.x*.5+.5,.5-p.y*.5);
}
#endif
#ifdef ICE_FRAGMENT_STAGE
in vec2 uv;
layout(location=0) out vec4 color;
uniform sampler2D ice_texture0,ice_texture1,ice_texture2,ice_texture3;
void scene_hdr() {
	bool top = uv.y < .45;
	color = top ? vec4(4,.5,.125,1) : vec4(.25,2,8,1);
	gl_FragDepth = top ? .25 : .75;
}
void threshold() { color = vec4(max(texture(ice_texture0,uv).rgb-1,0),1); }
void blur() {
	color = (texture(ice_texture0,uv-settings.xy)+texture(ice_texture0,uv)+
	         texture(ice_texture0,uv+settings.xy))/3;
}
void tone() {
	vec3 radiance = texture(ice_texture0,uv).rgb;
	color = vec4(radiance/(1+radiance),1);
}
void composite() {
	vec3 radiance = texture(ice_texture0,uv).rgb + .5*texture(ice_texture1,uv).rgb;
	color = vec4(radiance/(1+radiance),1);
}
void depth_probe() { color = vec4(vec3(texture(ice_texture2,uv).r),1); }
void bindings() {
	color = vec4(texture(ice_texture0,uv).r,texture(ice_texture1,uv).g,
	             texture(ice_texture2,uv).b,texture(ice_texture3,uv).r);
}
#endif
