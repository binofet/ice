#version 450
layout(location=0) in vec3 shade;
layout(location=0) out vec4 result_color;
void main() { result_color = vec4(shade, 1.0); }
