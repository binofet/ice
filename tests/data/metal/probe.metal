#include <metal_stdlib>
using namespace metal;
struct vertex_output { float4 position [[position]]; };
struct parameters { float4 color; };
vertex vertex_output probe_vertex(uint index [[vertex_id]]) {
	const float2 corners[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
	return {float4(corners[index], 0, 1)};
}
fragment float4 probe_fragment(vertex_output input [[stage_in]],
                               constant parameters &values [[buffer(0)]]) {
	return values.color;
}
fragment float4 probe_coordinates(vertex_output input [[stage_in]],
                                  constant parameters &values [[buffer(0)]]) {
	return float4(input.position.xy / values.color.xy, .25, 1);
}
fragment float4 unsupported_buffer(constant float4 &values [[buffer(1)]]) { return values; }
fragment float4 unsupported_texture(texture2d<float> image [[texture(0)]]) {
	return image.read(uint2(0));
}
