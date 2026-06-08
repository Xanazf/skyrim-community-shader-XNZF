cbuffer Params : register(b0)
{
	float letterboxHeight;
	float vignetteAmount;
	float enableLetterbox;
	float enableVignette;
	float timer;
	float3 padding;
};

RWTexture2D<float4> OutputTex : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint width, height;
	OutputTex.GetDimensions(width, height);

	if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
		return;

	float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(width, height);
	float4 color = OutputTex[dispatchThreadID.xy];

	// 1. Vignette
	if (enableVignette > 0.5) {
		float2 centerUV = uv - float2(0.5, 0.5);
		float dist = dot(centerUV, centerUV);
		float vignette = saturate(1.0 - dist * vignetteAmount);
		color.rgb *= vignette;
	}

	// 2. Letterbox
	if (enableLetterbox > 0.5) {
		if (uv.y < letterboxHeight || uv.y > (1.0 - letterboxHeight)) {
			color.rgb = 0.0;
		}
	}

	OutputTex[dispatchThreadID.xy] = color;
}
