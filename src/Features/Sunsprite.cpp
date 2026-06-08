#include "Sunsprite.h"
#include "Deferred.h"
#include "Globals.h"
#include <imgui.h>
#include <DDSTextureLoader.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	Sunsprite::Settings,
	EnableSunsprite,
	SunspriteIntensity,
	SunspriteSize)

void Sunsprite::RestoreDefaultSettings()
{
	settings = {};
}

void Sunsprite::LoadSettings(json& o_json)
{
	settings = o_json;
}

void Sunsprite::SaveSettings(json& o_json)
{
	o_json = settings;
}

void Sunsprite::DrawSettings()
{
	if (ImGui::TreeNodeEx("Sunsprite FX", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Sunsprite", &settings.EnableSunsprite);
		if (settings.EnableSunsprite) {
			ImGui::SliderFloat("Intensity", &settings.SunspriteIntensity, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("Size", &settings.SunspriteSize, 0.1f, 3.0f, "%.2f");
		}
		ImGui::TreePop();
	}
}

void Sunsprite::SetupResources()
{
	sunspriteParamsBuffer = new ConstantBuffer(ConstantBufferDesc<SunspriteParams>());

	auto device = globals::d3d::device;
	auto context = globals::d3d::context;

	if (std::filesystem::exists("Data\\Shaders\\Sunsprite\\enbsunsprite.dds")) {
		DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\Sunsprite\\enbsunsprite.dds", nullptr, sunspriteTextureView.put());
	} else {
		logger::warn("enbsunsprite.dds not found in Data\\Shaders\\Sunsprite\\");
	}
}

void Sunsprite::ClearShaderCache()
{
	if (sunspriteCS)
		sunspriteCS->Release();
	sunspriteCS = nullptr;
}

ID3D11ComputeShader* Sunsprite::GetSunspriteCS()
{
	if (!sunspriteCS) {
		logger::debug("Compiling SunspriteCS");
		sunspriteCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\Sunsprite\\SunspriteCS.hlsl", {}, "cs_5_0"));
	}
	return sunspriteCS;
}

void Sunsprite::RenderSunsprite(ID3D11ShaderResourceView* depthSRV, ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height)
{
	if (!settings.EnableSunsprite || !sunspriteTextureView)
		return;

	auto context = globals::d3d::context;
	auto shader = GetSunspriteCS();
	if (!shader)
		return;

	SunspriteParams params{
		.intensity = settings.SunspriteIntensity,
		.size = settings.SunspriteSize
	};
	sunspriteParamsBuffer->Update(params);

	context->CSSetShader(shader, nullptr, 0);

	ID3D11Buffer* cb0 = sunspriteParamsBuffer->CB();
	context->CSSetConstantBuffers(0, 1, &cb0);

	ID3D11Buffer* perFrameCB = *globals::game::perFrame;
	context->CSSetConstantBuffers(12, 1, &perFrameCB);

	if (globals::state && globals::state->sharedDataCB) {
		ID3D11Buffer* sdCB = globals::state->sharedDataCB->CB();
		context->CSSetConstantBuffers(5, 1, &sdCB);
	}

	ID3D11SamplerState* sampler = Deferred::GetSingleton()->linearSampler;
	context->CSSetSamplers(0, 1, &sampler);

	ID3D11ShaderResourceView* srvs[] = {
		depthSRV,
		sunspriteTextureView.get()
	};
	context->CSSetShaderResources(0, 2, srvs);

	ID3D11UnorderedAccessView* uavs[] = { outputUAV };
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	uint32_t dispatchX = (width + 7) / 8;
	uint32_t dispatchY = (height + 7) / 8;
	context->Dispatch(dispatchX, dispatchY, 1);

	// Cleanup
	context->CSSetShader(nullptr, nullptr, 0);
	ID3D11Buffer* nullCBs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	context->CSSetConstantBuffers(0, 13, nullCBs);

	ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
	context->CSSetShaderResources(0, 2, nullSRVs);

	ID3D11UnorderedAccessView* nullUAVs[] = { nullptr };
	context->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);

	ID3D11SamplerState* nullSampler = nullptr;
	context->CSSetSamplers(0, 1, &nullSampler);
}
