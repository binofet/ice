#include <metal_stdlib>
using namespace metal;
struct vertex_output { float4 position [[position]]; float2 uv; };
struct scene_output { float4 color [[color(0)]]; float depth [[depth(any)]]; };
struct parameters { float4 settings; };
constexpr sampler color_sampler(coord::normalized,address::clamp_to_edge,filter::linear);
constexpr sampler depth_sampler(coord::normalized,address::clamp_to_edge,filter::nearest);
vertex vertex_output fullscreen(uint index [[vertex_id]]) {
	const float2 corners[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
	float2 p = corners[index];
	return {float4(p,.5,1),float2(p.x*.5+.5,.5-p.y*.5)};
}
fragment scene_output scene_hdr(vertex_output input [[stage_in]]) {
	bool top = input.uv.y < .45;
	return {top ? float4(4,.5,.125,1) : float4(.25,2,8,1),top ? .25f : .75f};
}
fragment float4 threshold(vertex_output input [[stage_in]],texture2d<float> source [[texture(0)]]) {
	return float4(max(source.sample(color_sampler,input.uv).rgb-1,0),1);
}
fragment float4 blur(vertex_output input [[stage_in]],constant parameters &values [[buffer(0)]],
                     texture2d<float> source [[texture(0)]]) {
	float2 offset = values.settings.xy;
	return (source.sample(color_sampler,input.uv-offset)+source.sample(color_sampler,input.uv)+
	        source.sample(color_sampler,input.uv+offset))/3;
}
fragment float4 tone(vertex_output input [[stage_in]],texture2d<float> source [[texture(0)]]) {
	float3 radiance = source.sample(color_sampler,input.uv).rgb;
	return float4(radiance/(1+radiance),1);
}
fragment float4 composite(vertex_output input [[stage_in]],texture2d<float> source [[texture(0)]],
                          texture2d<float> bloom [[texture(1)]]) {
	float3 radiance = source.sample(color_sampler,input.uv).rgb +
	                  .5*bloom.sample(color_sampler,input.uv).rgb;
	return float4(radiance/(1+radiance),1);
}
fragment float4 depth_probe(vertex_output input [[stage_in]],depth2d<float> source [[texture(2)]]) {
	return float4(float3(source.sample(depth_sampler,input.uv)),1);
}
fragment float4 bindings(vertex_output input [[stage_in]],texture2d<float> first [[texture(0)]],
                         texture2d<float> second [[texture(1)]],texture2d<float> third [[texture(2)]],
                         texture2d<float> fourth [[texture(3)]]) {
	return float4(first.sample(color_sampler,input.uv).r,second.sample(color_sampler,input.uv).g,
	              third.sample(color_sampler,input.uv).b,fourth.sample(color_sampler,input.uv).r);
}
