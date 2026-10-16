#ifdef ICE_VERTEX_STAGE
layout(location = 0) in vec2 position;
void attribute_vertex() { gl_Position = vec4(position, 0, 1); }
void probe_vertex() { gl_Position = vec4(0, 0, 0, 1); }
#endif
#ifdef ICE_FRAGMENT_STAGE
layout(location = 0) out vec4 color;
uniform vec4 standalone;
uniform sampler2D image;
layout(std140) uniform first_block { vec4 first; };
layout(std140) uniform second_block { vec4 second; };
layout(std140) uniform huge_block { vec4 huge[1025]; };
void plain_fragment() { color = vec4(1); }
void uniform_fragment() { color = standalone; }
void texture_fragment() { color = texture(image, vec2(.5)); }
void blocks_fragment() { color = first + second; }
void oversized_fragment() { color = huge[1024] + huge[0]; }
#endif
