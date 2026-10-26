#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=0) out vec3 shade;
layout(push_constant) uniform Draw { mat4 transform; vec4 color; } params;
void main() {
    gl_Position = params.transform * vec4(position, 1.0);
    shade = params.color.rgb * clamp(0.5 + dot(normal, vec3(0.5, 0.8, 0.2)), 0.15, 1.0);
}
