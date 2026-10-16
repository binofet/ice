layout(std140) uniform data { vec4 values[1024]; };
#ifdef ICE_VERTEX_STAGE
layout(location = 0) in vec3 position;
layout(location = 2) in vec2 uv;
out vec2 coordinate;
void     vertex_main() {
    coordinate    = uv;
    gl_Position   = vec4(position + values[0].xyz, 1);
    gl_Position.z = 2.0 * gl_Position.z - gl_Position.w;
}
#else
in vec2           coordinate;
uniform sampler2D ice_texture0, ice_texture1, ice_texture2, ice_texture3;
out vec4          color;
void              fragment_main() {
    color = (texture(ice_texture0, coordinate) + texture(ice_texture1, coordinate) +
             texture(ice_texture2, coordinate) + texture(ice_texture3, coordinate)) *
                .25 +
            values[1023];
}
#endif
