#ifdef ICE_VERTEX_STAGE
void probe_vertex() {
	const vec2 positions[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
	gl_Position = vec4(positions[gl_VertexID], 0, 1);
}
#endif
#ifdef ICE_FRAGMENT_STAGE
layout(std140) uniform parameters { vec4 values; };
layout(location = 0) out vec4 color;
void probe_fragment() { color = values; }
void probe_coordinates() { color = vec4(gl_FragCoord.x / values.x, 1 - gl_FragCoord.y / values.y, .25, 1); }
#endif
