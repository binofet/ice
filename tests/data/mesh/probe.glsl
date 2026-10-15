layout(std140) uniform parameters { mat4 transform; vec4 tint; vec4 light; };
#ifdef ICE_VERTEX_STAGE
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
out vec3 surface_normal;
void mesh_vertex_main() {
	gl_Position = transform * vec4(position, 1);
	// The shared scene contract is zero-to-one depth. This authored GL entry adapts it.
	gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
	surface_normal = normal;
}
void overlay_vertex() {
	const vec2 corners[3] = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
	gl_Position = vec4(corners[gl_VertexID], 0, 1);
	surface_normal = vec3(0,0,1);
}
#endif
#ifdef ICE_FRAGMENT_STAGE
in vec3 surface_normal;
layout(location=0) out vec4 color;
void mesh_fragment() {
	float brightness = .25 + .75 * max(dot(normalize(surface_normal), normalize(light.xyz)), 0);
	color = vec4(tint.rgb * brightness, tint.a);
}
uniform sampler2D canvas;
void overlay_texture() {
	color = texture(canvas, vec2(gl_FragCoord.x / light.x, 1.0 - gl_FragCoord.y / light.y));
}
#endif
