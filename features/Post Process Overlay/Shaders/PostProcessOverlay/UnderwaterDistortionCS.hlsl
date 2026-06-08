cbuffer Params : register(b0)
{
	float strength;
	float speed;
	float timer;
	float padding;
};

Texture2D<float4> SourceTex : register(t0);
SamplerState LinearSampler : register(s0);

RWTexture2D<float4> OutputTex : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint width, height;
	OutputTex.GetDimensions(width, height);

	if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
		return;

	float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(width, height);

	// Procedural wavy screen-space distortion mirroring ENB's enbunderwater.fx:
	// overlapping sine waves traveling across the screen in both axes, scaled
	// by user strength and animated by the global timer.
	float t = timer * speed;
	float2 wave;
	wave.x = sin(uv.y * 18.0 + t * 1.7) * 0.6 + sin(uv.y * 7.0 - t * 0.9) * 0.4;
	wave.y = sin(uv.x * 14.0 + t * 1.3) * 0.6 + sin(uv.x * 5.0 + t * 1.1) * 0.4;

	float2 distortedUV = saturate(uv + wave * strength * 0.0075);

	OutputTex[dispatchThreadID.xy] = SourceTex.SampleLevel(LinearSampler, distortedUV, 0);
}
