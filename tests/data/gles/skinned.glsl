layout(std140) uniform data { vec4 values[1024]; };
#ifdef ICE_VERTEX_STAGE
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 joints;
layout(location = 4) in vec4 weights;
out vec2 coordinate;
void     vertex_main() {
    coordinate = uv;
    vec3 offset = values[int(joints.x)].xyz * weights.x + values[int(joints.y)].xyz * weights.y +
                  values[int(joints.z)].xyz * weights.z + values[int(joints.w)].xyz * weights.w;
    gl_Position   = vec4(position + offset + normal * values[1022].x, 1);
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
