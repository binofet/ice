layout(std140) uniform data { vec4 values[1024]; };
#ifdef ICE_VERTEX_STAGE
layout(location = 0) in vec3 position;
void vertex_main() {
	gl_Position   = vec4(position + values[0].xyz, 1);
	gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
}
#else
out vec4 color;
void     fragment_main() { color = values[1023]; }
#endif
