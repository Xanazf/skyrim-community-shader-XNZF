#include "DepthOfField.h"
#include <imgui.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	DepthOfField::Settings,
	FocusSpeed,
	Aperture)

void DepthOfField::RestoreDefaultSettings()
{
	settings = {};
}

void DepthOfField::LoadSettings(json& o_json)
{
	settings = o_json;
}

void DepthOfField::SaveSettings(json& o_json)
{
	o_json = settings;
}

void DepthOfField::DrawSettings()
{
	ImGui::SliderFloat("Focus Speed", &settings.FocusSpeed, 0.1f, 5.0f);
	ImGui::SliderFloat("Aperture", &settings.Aperture, 0.1f, 1.0f);
}

void DepthOfField::SetupResources()
{
	// Load CS compute shaders for DepthOfField passes here
}

void DepthOfField::Prepass()
{
	// Execute DoF compute shader passes here.
	// This would bind the Depth buffer and Color buffer, dispatch, and blur.
}