#include <metal_stdlib>
using namespace metal;
struct parameters { float4x4 transform; float4 tint; float4 light; };
struct mesh_input { float3 position [[attribute(0)]]; float3 normal [[attribute(1)]]; };
struct mesh_output { float4 position [[position]]; float3 normal; };
vertex mesh_output mesh_vertex_main(mesh_input input [[stage_in]],
                                    constant parameters &values [[buffer(0)]]) {
	return {values.transform * float4(input.position, 1), input.normal};
}
vertex mesh_output overlay_vertex(uint index [[vertex_id]]) {
	const float2 corners[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
	return {float4(corners[index],0,1), float3(0,0,1)};
}
fragment float4 mesh_fragment(mesh_output input [[stage_in]],
                              constant parameters &values [[buffer(0)]]) {
	float brightness = .25 + .75 * max(dot(normalize(input.normal), normalize(values.light.xyz)), 0.0);
	return float4(values.tint.rgb * brightness, values.tint.a);
}
fragment float4 overlay_texture(mesh_output input [[stage_in]],
    constant parameters &values [[buffer(0)]], texture2d<float> canvas [[texture(0)]]) {
	constexpr sampler linear_clamp(coord::normalized, address::clamp_to_edge, filter::linear);
	return canvas.sample(linear_clamp, input.position.xy / values.light.xy);
}
