#pragma once

#include "Feature.h"

struct DepthOfField : public Feature
{
	virtual inline std::string GetName() override { return "Depth Of Field"; }
	virtual inline std::string GetShortName() override { return "DepthOfField"; }
	virtual inline std::string_view GetCategory() const override { return "Post Process"; }
	virtual inline bool SupportsVR() override { return true; }
	virtual inline bool IsCore() const override { return false; }

	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return {
			"Overrides the engine's native Depth of Field strength, focal distance, and range.",
			{
				"Lets users tune or disable Depth of Field globally, regardless of the active weather or interior cell.",
				"Applies on top of the engine's existing native DoF rendering; no additional GPU cost."
			}
		};
	}

	struct Settings
	{
		bool EnableOverride = false;
		float Strength = 1.0f;
		float Distance = 4096.0f;
		float Range = 4096.0f;
	};

	Settings settings;

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;

	virtual void Prepass() override;
};
