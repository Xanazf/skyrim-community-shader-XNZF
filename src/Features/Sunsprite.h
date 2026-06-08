#pragma once

#include "Feature.h"
#include "Buffer.h"
#include <winrt/base.h>

struct Sunsprite : public Feature
{
public:
	virtual inline std::string GetName() override { return "Sunsprite FX"; }
	virtual inline std::string GetShortName() override { return "Sunsprite"; }
	virtual inline std::string_view GetCategory() const override { return "Post Process"; }
	virtual inline bool SupportsVR() override { return true; }
	virtual inline bool IsCore() const override { return false; }

	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return {
			"Provides dynamic screen-space Sunsprite flares.",
			{
				"Calculates sun occlusion dynamically using the main depth buffer.",
				"Projects flares along the sun vector to the screen center.",
				"Uses enbsunsprite.dds for customizable artistic flares."
			}
		};
	}

	struct Settings
	{
		bool EnableSunsprite = true;
		float SunspriteIntensity = 1.0f;
		float SunspriteSize = 1.0f;
	};

	Settings settings;

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;

	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;

	ID3D11ComputeShader* GetSunspriteCS();
	void RenderSunsprite(ID3D11ShaderResourceView* depthSRV, ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height);

private:
	winrt::com_ptr<ID3D11ShaderResourceView> sunspriteTextureView;
	ID3D11ComputeShader* sunspriteCS = nullptr;

	struct SunspriteParams
	{
		float intensity;
		float size;
		float padding[2];
	};

	ConstantBuffer* sunspriteParamsBuffer = nullptr;
};
