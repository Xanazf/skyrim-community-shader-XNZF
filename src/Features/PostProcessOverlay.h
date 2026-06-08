#pragma once

#include "Feature.h"
#include "Buffer.h"

struct PostProcessOverlay : public Feature
{
	virtual inline std::string GetName() override { return "Post Process Overlay"; }
	virtual inline std::string GetShortName() override { return "PostProcessOverlay"; }
	virtual inline std::string_view GetCategory() const override { return "Post Process"; }
	virtual inline bool SupportsVR() override { return false; }
	virtual inline bool IsCore() const override { return false; }

	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return {
			"Provides screen-space overlays ported from ENB (Letterbox, Vignette, Underwater Distortion, etc).",
			{
				"Applies Letterboxing (black bars).",
				"Applies Vignetting (darkened corners).",
				"Applies a wavy screen-space distortion while the camera is underwater."
			}
		};
	}

	struct Settings
	{
		bool EnableLetterbox = false;
		float LetterboxHeight = 0.1f;
		bool EnableVignette = false;
		float VignetteAmount = 1.0f;
		bool EnablePostpass = false;
		bool EnableUnderwaterDistortion = false;
		float UnderwaterDistortionStrength = 1.0f;
		float UnderwaterDistortionSpeed = 1.0f;
	};

	Settings settings;

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;

	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;

	ID3D11ComputeShader* GetPostProcessCS();
	ID3D11ComputeShader* GetPostpassCS();
	ID3D11ComputeShader* GetUnderwaterDistortionCS();

	// Lazily (re)creates frameCopyTexture to match outputUAV's format/size,
	// copies outputUAV's current contents into it, and returns its SRV --
	// ready to bind alongside a linear sampler for filtered neighborhood
	// sampling (chromatic aberration, blur/distortion, bokeh-style gathers,
	// etc) without an in-place read/write hazard on outputUAV itself.
	// Callers must re-fetch immediately before their own dispatch so the
	// copy reflects that pass's actual input -- i.e. whatever the previous
	// pass composited, not the original pre-overlay frame.
	ID3D11ShaderResourceView* GetFrameCopySRV(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height);

	// Hooking into post-processing right before presentation
	void Present(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height);

private:
	ID3D11ComputeShader* postProcessCS = nullptr;
	ID3D11ComputeShader* postpassCS = nullptr;
	ID3D11ComputeShader* underwaterDistortionCS = nullptr;

	struct PostProcessParams
	{
		float letterboxHeight;
		float vignetteAmount;
		float enableLetterbox;
		float enableVignette;
		float timer;
		float padding[3];
	};

	struct UnderwaterParams
	{
		float strength;
		float speed;
		float timer;
		float padding;
	};

	ConstantBuffer* postProcessParamsBuffer = nullptr;
	ConstantBuffer* underwaterParamsBuffer = nullptr;

	// Shared scratch copy of the current frame, (re)written by GetFrameCopySRV
	// for whichever pass needs filtered input this frame.
	Texture2D* frameCopyTexture = nullptr;

	void RenderUnderwaterDistortion(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height);
};
