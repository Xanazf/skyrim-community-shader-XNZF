cbuffer Params : register(b0)
{
	float threshold;
	float intensity;
	float glareIntensity;
	float dirtIntensity;
	uint passType; // 0 = Extract, 1 = Downsample, 2 = Upsample, 3 = Glare/Dirt/Composite
	float3 padding;
};

Texture2D<float4> InputTex : register(t0);
Texture2D<float4> InputTex2 : register(t1);

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> OutputTex : register(u0);

float ColorToLuminance(float3 c)
{
	return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float3 ExtractHighlight(float3 color, float emissive, float thresh)
{
	float brightness = max(color.r, max(color.g, color.b));
	float thresholdFactor = max(0.0, brightness - thresh) / max(brightness, 0.0001);
	// Boost highlights using emissive channel
	return color * max(thresholdFactor, emissive);
}

float3 Downsample13Tap(Texture2D<float4> tex, SamplerState sam, float2 uv, float2 texelSize)
{
	// 13-tap dual filtering downsample
	float3 A = tex.SampleLevel(sam, uv + texelSize * float2(-2.0, -2.0), 0).rgb;
	float3 B = tex.SampleLevel(sam, uv + texelSize * float2( 0.0, -2.0), 0).rgb;
	float3 C = tex.SampleLevel(sam, uv + texelSize * float2( 2.0, -2.0), 0).rgb;
	float3 D = tex.SampleLevel(sam, uv + texelSize * float2(-1.0, -1.0), 0).rgb;
	float3 E = tex.SampleLevel(sam, uv + texelSize * float2( 1.0, -1.0), 0).rgb;
	float3 F = tex.SampleLevel(sam, uv + texelSize * float2(-2.0,  0.0), 0).rgb;
	float3 G = tex.SampleLevel(sam, uv + texelSize * float2( 0.0,  0.0), 0).rgb;
	float3 H = tex.SampleLevel(sam, uv + texelSize * float2( 2.0,  0.0), 0).rgb;
	float3 I = tex.SampleLevel(sam, uv + texelSize * float2(-1.0,  1.0), 0).rgb;
	float3 J = tex.SampleLevel(sam, uv + texelSize * float2( 1.0,  1.0), 0).rgb;
	float3 K = tex.SampleLevel(sam, uv + texelSize * float2(-2.0,  2.0), 0).rgb;
	float3 L = tex.SampleLevel(sam, uv + texelSize * float2( 0.0,  2.0), 0).rgb;
	float3 M = tex.SampleLevel(sam, uv + texelSize * float2( 2.0,  2.0), 0).rgb;

	float3 g1 = (A + B + D + G) * 0.25;
	float3 g2 = (B + C + G + E) * 0.25;
	float3 g3 = (D + G + K + L) * 0.25;
	float3 g4 = (G + E + L + M) * 0.25;
	float3 g5 = (D + E + I + J) * 0.25;

	float w1 = 1.0 / (1.0 + ColorToLuminance(g1));
	float w2 = 1.0 / (1.0 + ColorToLuminance(g2));
	float w3 = 1.0 / (1.0 + ColorToLuminance(g3));
	float w4 = 1.0 / (1.0 + ColorToLuminance(g4));
	float w5 = 1.0 / (1.0 + ColorToLuminance(g5));

	float3 result = (g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4 + g5 * w5) / (w1 + w2 + w3 + w4 + w5);
	return result;
}

float3 UpsampleTent(Texture2D<float4> tex, SamplerState sam, float2 uv, float2 texelSize)
{
	float4 d = texelSize.xyxy * float4(-1.0, -1.0, 1.0, 1.0);

	float3 s00 = tex.SampleLevel(sam, uv + d.xy, 0).rgb;
	float3 s10 = tex.SampleLevel(sam, uv + float2(0.0, d.y), 0).rgb;
	float3 s20 = tex.SampleLevel(sam, uv + float2(d.z, d.y), 0).rgb;

	float3 s01 = tex.SampleLevel(sam, uv + float2(d.x, 0.0), 0).rgb;
	float3 s11 = tex.SampleLevel(sam, uv + float2(0.0, 0.0), 0).rgb;
	float3 s21 = tex.SampleLevel(sam, uv + float2(d.z, 0.0), 0).rgb;

	float3 s02 = tex.SampleLevel(sam, uv + float2(d.x, d.w), 0).rgb;
	float3 s12 = tex.SampleLevel(sam, uv + float2(0.0, d.w), 0).rgb;
	float3 s22 = tex.SampleLevel(sam, uv + float2(d.z, d.w), 0).rgb;

	float3 result =
		(s00 + s20 + s02 + s22) * (1.0 / 16.0) +
		(s10 + s01 + s21 + s12) * (2.0 / 16.0) +
		(s11) * (4.0 / 16.0);

	return result;
}

float3 GetStarburstGlare(Texture2D<float4> tex, SamplerState sam, float2 uv)
{
	float3 glare = 0.0;
	float2 directions[4] = {
		float2(1.0, 0.0),
		float2(0.0, 1.0),
		float2(0.707, 0.707),
		float2(-0.707, 0.707)
	};

	for (int d = 0; d < 4; ++d) {
		float2 dir = directions[d] * 0.004;
		glare += tex.SampleLevel(sam, uv + dir * 1.0, 0).rgb * 0.25;
		glare += tex.SampleLevel(sam, uv - dir * 1.0, 0).rgb * 0.25;
		glare += tex.SampleLevel(sam, uv + dir * 2.0, 0).rgb * 0.15;
		glare += tex.SampleLevel(sam, uv - dir * 2.0, 0).rgb * 0.15;
		glare += tex.SampleLevel(sam, uv + dir * 3.0, 0).rgb * 0.10;
		glare += tex.SampleLevel(sam, uv - dir * 3.0, 0).rgb * 0.10;
	}
	return glare * 0.25;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint width, height;
	OutputTex.GetDimensions(width, height);

	if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
		return;

	float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(width, height);
	float2 texelSize = 1.0 / float2(width, height);

	float3 color = 0.0;

	if (passType == 0) {
		// Highlight extraction + downsample from input texture
		// Sample 13 taps
		float4 sA = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2(-2.0, -2.0), 0);
		float4 sB = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 0.0, -2.0), 0);
		float4 sC = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 2.0, -2.0), 0);
		float4 sD = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2(-1.0, -1.0), 0);
		float4 sE = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 1.0, -1.0), 0);
		float4 sF = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2(-2.0,  0.0), 0);
		float4 sG = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 0.0,  0.0), 0);
		float4 sH = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 2.0,  0.0), 0);
		float4 sI = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2(-1.0,  1.0), 0);
		float4 sJ = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 1.0,  1.0), 0);
		float4 sK = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2(-2.0,  2.0), 0);
		float4 sL = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 0.0,  2.0), 0);
		float4 sM = InputTex.SampleLevel(LinearSampler, uv + texelSize * float2( 2.0,  2.0), 0);

		float3 A = ExtractHighlight(sA.rgb, sA.a, threshold);
		float3 B = ExtractHighlight(sB.rgb, sB.a, threshold);
		float3 C = ExtractHighlight(sC.rgb, sC.a, threshold);
		float3 D = ExtractHighlight(sD.rgb, sD.a, threshold);
		float3 E = ExtractHighlight(sE.rgb, sE.a, threshold);
		float3 F = ExtractHighlight(sF.rgb, sF.a, threshold);
		float3 G = ExtractHighlight(sG.rgb, sG.a, threshold);
		float3 H = ExtractHighlight(sH.rgb, sH.a, threshold);
		float3 I = ExtractHighlight(sI.rgb, sI.a, threshold);
		float3 J = ExtractHighlight(sJ.rgb, sJ.a, threshold);
		float3 K = ExtractHighlight(sK.rgb, sK.a, threshold);
		float3 L = ExtractHighlight(sL.rgb, sL.a, threshold);
		float3 M = ExtractHighlight(sM.rgb, sM.a, threshold);

		float3 g1 = (A + B + D + G) * 0.25;
		float3 g2 = (B + C + G + E) * 0.25;
		float3 g3 = (D + G + K + L) * 0.25;
		float3 g4 = (G + E + L + M) * 0.25;
		float3 g5 = (D + E + I + J) * 0.25;

		float w1 = 1.0 / (1.0 + ColorToLuminance(g1));
		float w2 = 1.0 / (1.0 + ColorToLuminance(g2));
		float w3 = 1.0 / (1.0 + ColorToLuminance(g3));
		float w4 = 1.0 / (1.0 + ColorToLuminance(g4));
		float w5 = 1.0 / (1.0 + ColorToLuminance(g5));

		color = (g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4 + g5 * w5) / (w1 + w2 + w3 + w4 + w5);
	} else if (passType == 1) {
		// Normal downsampling step
		color = Downsample13Tap(InputTex, LinearSampler, uv, texelSize);
	} else if (passType == 2) {
		// Upsampling step
		float3 upsampled = UpsampleTent(InputTex, LinearSampler, uv, texelSize);
		float3 downsampled = InputTex2.SampleLevel(LinearSampler, uv, 0).rgb;
		color = downsampled + upsampled;
	} else if (passType == 3) {
		// Glare/Lens composites
		float3 bloomVal = InputTex.SampleLevel(LinearSampler, uv, 0).rgb;
		float3 lensDirt = InputTex2.SampleLevel(LinearSampler, uv, 0).rgb;

		color = bloomVal * intensity;
		if (glareIntensity > 0.0) {
			color += GetStarburstGlare(InputTex, LinearSampler, uv) * glareIntensity;
		}
		if (dirtIntensity > 0.0) {
			color += bloomVal * lensDirt * dirtIntensity;
		}
	}

	OutputTex[dispatchThreadID.xy] = float4(color, 1.0);
}
