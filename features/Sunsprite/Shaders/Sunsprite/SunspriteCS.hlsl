#include "Common/FrameBuffer.hlsli"
#include "Common/VR.hlsli"

cbuffer Params : register(b0)
{
	float intensity;
	float size;
	float2 padding;
};

// SharedData is bound to b5
cbuffer SharedData : register(b5)
{
	float4 WaterData[25];
	row_major float3x4 DirectionalAmbient;
	float4 DirLightDirection;
	float4 DirLightColor;
	float4 CameraData;
	float4 BufferDim;
	float Timer;
	uint FrameCount;
	uint FrameCountAlwaysActive;
	bool InInterior;
	bool HasDirectionalShadows;
	bool InMapMenu;
	bool HideSky;
	float MipBias;
	float WaterSystemHeight;
	float3 pad0;
	float4 AmbientSHR;
	float4 AmbientSHG;
	float4 AmbientSHB;
	float4 HDRData;
};

Texture2D<float4> DepthTex : register(t0);
Texture2D<float4> SunspriteTex : register(t1);

SamplerState LinearSampler : register(s0);

RWTexture2D<float4> OutputTex : register(u0);

struct FlareElement
{
	float offset;
	float size;
	float3 color;
	float2 uvMin;
	float2 uvMax;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint width, height;
	OutputTex.GetDimensions(width, height);

	if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
		return;

	// In interior, sun is not visible
	if (InInterior)
		return;

	uint eyeIndex = 0;
#if defined(VR)
	eyeIndex = dispatchThreadID.x >= (width / 2) ? 1 : 0;
#endif

	// 1. Calculate projected sun position
	float3 sunWS = DirLightDirection.xyz * 100000.0;
	float4 sunCS = mul(FrameBuffer::CameraViewProj[eyeIndex], float4(sunWS, 1.0));

	// Behind camera
	if (sunCS.w <= 0.0)
		return;

	float2 sunNDC = sunCS.xy / sunCS.w;
	float2 sunUV = sunNDC * float2(0.5, -0.5) + 0.5;

#if defined(VR)
	// Adjust sunUV for eye bounds in VR
	if (eyeIndex == 0) {
		sunUV.x *= 0.5;
	} else {
		sunUV.x = 0.5 + sunUV.x * 0.5;
	}
#endif

	// Clamp to dynamic resolution if active
	float2 sunUV_DR = FrameBuffer::GetDynamicResolutionAdjustedScreenPosition(sunUV);

	// 2. Compute smooth sun visibility/occlusion
	float visibility = 0.0;
	float2 offsets[9] = {
		float2(0.0, 0.0),
		float2(-0.01, -0.01), float2(0.01, -0.01),
		float2(-0.01, 0.01), float2(0.01, 0.01),
		float2(0.0, -0.015), float2(0.0, 0.015),
		float2(-0.015, 0.0), float2(0.015, 0.0)
	};

	for (int i = 0; i < 9; ++i) {
		float2 uvSample = sunUV_DR + offsets[i] * size * 0.05;
		float d = DepthTex.SampleLevel(LinearSampler, uvSample, 0).x;
		// Reversed-Z: 0.0 is sky/far plane
		if (d < 0.0001) {
			visibility += 1.0;
		}
	}
	visibility /= 9.0;

	if (visibility <= 0.001)
		return;

	// 3. Render flares along vector to center of current eye's screen
	float2 currentUV = (float2(dispatchThreadID.xy) + 0.5) / float2(width, height);
	float2 screenCenter = float2(0.5, 0.5);
#if defined(VR)
	if (eyeIndex == 0) {
		screenCenter.x = 0.25;
	} else {
		screenCenter.x = 0.75;
	}
#endif

	float2 sunToCenter = screenCenter - sunUV;

	FlareElement elements[5] = {
		{ 0.0,  0.5,  float3(1.0, 0.9, 0.7), float2(0.0, 0.0), float2(0.5, 0.5) }, // Corona
		{ 0.4,  0.2,  float3(0.6, 0.85, 1.0), float2(0.5, 0.0), float2(1.0, 0.5) }, // Ring
		{ 0.8,  0.3,  float3(0.9, 0.7, 1.0), float2(0.0, 0.5), float2(0.5, 1.0) }, // Starburst
		{ 1.3,  0.4,  float3(1.0, 0.75, 0.6), float2(0.5, 0.5), float2(1.0, 1.0) }, // Halo
		{ -0.3, 0.25, float3(0.7, 1.0, 0.7), float2(0.0, 0.0), float2(0.5, 0.5) }  // Secondary
	};

	float3 flareAcc = 0.0;
	float aspect = float(width) / float(height);

	for (int j = 0; j < 5; ++j) {
		float2 flarePos = sunUV + sunToCenter * elements[j].offset;
		float2 toPixel = currentUV - flarePos;
		// Correct aspect ratio for circular flares
		toPixel.x *= aspect;

		float halfSz = elements[j].size * size * 0.15;
		if (abs(toPixel.x) < halfSz && abs(toPixel.y) < halfSz) {
			float2 localUV = (toPixel / halfSz) * 0.5 + 0.5;
			float2 texUV = lerp(elements[j].uvMin, elements[j].uvMax, localUV);
			float3 sampled = SunspriteTex.SampleLevel(LinearSampler, texUV, 0).rgb;
			flareAcc += sampled * elements[j].color * intensity * visibility;
		}
	}

	if (any(flareAcc > 0.0)) {
		float4 sceneColor = OutputTex[dispatchThreadID.xy];
		OutputTex[dispatchThreadID.xy] = float4(sceneColor.rgb + flareAcc, sceneColor.a);
	}
}
