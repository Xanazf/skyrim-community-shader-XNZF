#include "DepthOfField.h"
#include "Util.h"
#include <imgui.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	DepthOfField::Settings,
	EnableOverride,
	Strength,
	Distance,
	Range)

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
	if (ImGui::TreeNodeEx("Depth Of Field", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Override Depth of Field", &settings.EnableOverride);
		if (settings.EnableOverride) {
			ImGui::SliderFloat("Strength", &settings.Strength, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Focal Distance", &settings.Distance, 0.0f, 50000.0f, "%.1f");
			ImGui::SliderFloat("Focal Range", &settings.Range, 0.0f, 50000.0f, "%.1f");
		}
		ImGui::TreePop();
	}
}

void DepthOfField::Prepass()
{
	if (!settings.EnableOverride)
		return;

	auto imageSpaceManager = RE::ImageSpaceManager::GetSingleton();
	if (!imageSpaceManager)
		return;

	GET_INSTANCE_MEMBER(currentBaseData, imageSpaceManager)
	if (!currentBaseData)
		return;

	currentBaseData->depthOfField.strength = settings.Strength;
	currentBaseData->depthOfField.distance = settings.Distance;
	currentBaseData->depthOfField.range = settings.Range;
}
