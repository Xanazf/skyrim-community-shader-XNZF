#pragma once

#include "Feature.h"
#include "Buffer.h"

struct DepthOfField : public Feature
{
	virtual inline std::string GetName() override { return "Depth Of Field"; }
	virtual inline std::string GetShortName() override { return "DepthOfField"; }
	virtual inline std::string_view GetCategory() const override { return "Post Process"; }
	virtual inline bool SupportsVR() override { return false; }
	virtual inline bool IsCore() const override { return false; }

	virtual std::pair<std::string, std::vector<std::string>> GetFeatureSummary() override
	{
		return {
			"Provides a framework for executing Depth of Field shaders ported from ENB.",
			{
				"Reads from main depth buffer to calculate focus.",
				"Applies bokeh and blur passes before UI rendering."
			}
		};
	}

	struct Settings
	{
		// Translated ENB DoF settings can go here.
		// Since DoF .fx files are highly customized per preset, these represent
		// a common baseline or can be extended by preset authors.
		float FocusSpeed = 1.0f;
		float Aperture = 0.5f;
	};

	Settings settings;

	virtual void RestoreDefaultSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void DrawSettings() override;

	virtual void Prepass() override;
	virtual void SetupResources() override;
};
