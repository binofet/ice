#include <metal_stdlib>
using namespace metal;
struct frame_parameters {
	float4 time_power_size;
	float4 gesture;
};
vertex float4 companion_vertex(uint index [[vertex_id]]) {
	const float2 corners[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
	return float4(corners[index], 0, 1);
}
fragment float4 companion_fragment(float4 pixel [[position]],
	constant frame_parameters &frame [[buffer(0)]]) {
	float time = frame.time_power_size.x;
	float power = clamp(frame.time_power_size.y, 0.0, 1.0);
	float2 size = max(frame.time_power_size.zw, float2(1));
	float2 uv = pixel.xy / size;
	float2 p = (uv - float2(.5 + frame.gesture.x * .25, .72 - power * .15))
		* float2(size.x / size.y, 1);
	float radius = length(p);
	// Low contrast keeps the native controller labels readable at every backing size.
	float wave = .5 + .5 * cos(radius * 48 - time * 1.5);
	float ring = exp(-abs(radius - .18 - power * .12) * 85);
	float3 color = float3(.018, .035, .055);
	color += float3(.012, .035, .045) * wave * exp(-radius * 3);
	color += float3(.015, .13, .16) * ring * (.35 + power);
	return float4(color, 1);
}
