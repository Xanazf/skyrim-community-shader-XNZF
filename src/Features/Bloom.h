#pragma once

#include "Feature.h"
#include "Buffer.h"
#include <winrt/base.h>

struct Bloom : public Feature
{
public:
	virtual inline std::string GetName() override { return "Bloom & Lens"; }
	virtual inline std::string GetShortName() override { return "Bloom"; }
	virtual inline std::string_view GetCategory() const override { return "Post Process"; }
	virtual inline bool SupportsVR() override { return false; }
	virtual inline bool IsCore() const override { return false; }

	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return {
			"Provides physically-based Bloom and screen-space Lens FX.",
			{
				"Physically-based highlight extraction and Karis downsampling/upsampling.",
				"Diffraction Starburst Glare around bright spots.",
				"Exposure-aware lens dirt overlay using lensmask.dds."
			}
		};
	}

	struct Settings
	{
		bool EnableBloom = true;
		float BloomThreshold = 1.0f;
		float BloomIntensity = 1.0f;
		bool EnableLens = true;
		float GlareIntensity = 0.5f;
		float DirtIntensity = 0.5f;
	};

	Settings settings;

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;

	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;

	ID3D11ComputeShader* GetBloomCS();
	void RenderBloom(ID3D11ShaderResourceView* preTonemapSRV, uint32_t width, uint32_t height);

	ID3D11ShaderResourceView* GetBloomTextureSRV() const
	{
		return bloomUpsampleTextures[0] ? bloomUpsampleTextures[0]->srv.get() : nullptr;
	}

private:
	static constexpr uint32_t MIP_COUNT = 4;

	Texture2D* bloomDownsampleTextures[MIP_COUNT]{ nullptr };
	Texture2D* bloomUpsampleTextures[MIP_COUNT]{ nullptr };

	winrt::com_ptr<ID3D11ShaderResourceView> lensDirtView;
	ID3D11ComputeShader* bloomCS = nullptr;

	struct BloomParams
	{
		float threshold;
		float intensity;
		float glareIntensity;
		float dirtIntensity;
		uint32_t passType; // 0 = Extract, 1 = Downsample, 2 = Upsample, 3 = Glare/Dirt/Composite
		float padding[3];
	};

	ConstantBuffer* bloomParamsBuffer = nullptr;
};
