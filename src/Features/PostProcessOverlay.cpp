#include "PostProcessOverlay.h"
#include "Globals.h"
#include "State.h"
#include <imgui.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PostProcessOverlay::Settings,
	EnableLetterbox,
	LetterboxHeight,
	EnableVignette,
	VignetteAmount,
	EnablePostpass)

void PostProcessOverlay::RestoreDefaultSettings()
{
	settings = {};
}

void PostProcessOverlay::LoadSettings(json& o_json)
{
	settings = o_json;
}

void PostProcessOverlay::SaveSettings(json& o_json)
{
	o_json = settings;
}

void PostProcessOverlay::DrawSettings()
{
	ImGui::Checkbox("Enable Letterbox", &settings.EnableLetterbox);
	if (settings.EnableLetterbox) {
		ImGui::SliderFloat("Letterbox Height", &settings.LetterboxHeight, 0.0f, 0.5f);
	}

	ImGui::Spacing();

	ImGui::Checkbox("Enable Vignette", &settings.EnableVignette);
	if (settings.EnableVignette) {
		ImGui::SliderFloat("Vignette Amount", &settings.VignetteAmount, 0.0f, 5.0f);
	}

	ImGui::Spacing();

	bool oldPostpass = settings.EnablePostpass;
	if (ImGui::Checkbox("Enable Custom Postpass (PostProcess.hlsl)", &settings.EnablePostpass)) {
		if (settings.EnablePostpass != oldPostpass && !settings.EnablePostpass) {
			// Clear compiled shader on disable
			if (postpassCS) {
				postpassCS->Release();
				postpassCS = nullptr;
			}
		}
	}
}

void PostProcessOverlay::SetupResources()
{
	postProcessParamsBuffer = new ConstantBuffer(ConstantBufferDesc<PostProcessParams>());
}

void PostProcessOverlay::ClearShaderCache()
{
	if (postProcessCS) {
		postProcessCS->Release();
		postProcessCS = nullptr;
	}
	if (postpassCS) {
		postpassCS->Release();
		postpassCS = nullptr;
	}
}

ID3D11ComputeShader* PostProcessOverlay::GetPostProcessCS()
{
	if (!postProcessCS) {
		logger::debug("Compiling PostProcessCS");
		postProcessCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\PostProcessOverlay\\PostProcessCS.hlsl", {}, "cs_5_0"));
	}
	return postProcessCS;
}

ID3D11ComputeShader* PostProcessOverlay::GetPostpassCS()
{
	if (!postpassCS && settings.EnablePostpass) {
		std::filesystem::path path = L"Data\\Shaders\\PostProcess.hlsl";
		if (std::filesystem::exists(path)) {
			logger::info("Compiling dynamic PostProcess.hlsl");
			postpassCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(path.c_str(), {}, "cs_5_0"));
		} else {
			logger::warn("PostProcess.hlsl not found at Data\\Shaders\\PostProcess.hlsl");
		}
	}
	return postpassCS;
}

void PostProcessOverlay::Present(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height)
{
	if (!settings.EnableLetterbox && !settings.EnableVignette && !settings.EnablePostpass)
		return;

	auto context = globals::d3d::context;

	// 1. Dispatch custom PostProcess.hlsl if enabled and compiled
	if (settings.EnablePostpass) {
		auto postpassShader = GetPostpassCS();
		if (postpassShader) {
			context->CSSetShader(postpassShader, nullptr, 0);

			PostProcessParams params{
				.letterboxHeight = settings.LetterboxHeight,
				.vignetteAmount = settings.VignetteAmount,
				.enableLetterbox = settings.EnableLetterbox ? 1.0f : 0.0f,
				.enableVignette = settings.EnableVignette ? 1.0f : 0.0f,
				.timer = globals::state ? globals::state->timer : 0.0f
			};
			postProcessParamsBuffer->Update(params);
			ID3D11Buffer* cb0 = postProcessParamsBuffer->CB();
			context->CSSetConstantBuffers(0, 1, &cb0);

			if (globals::state && globals::state->sharedDataCB) {
				ID3D11Buffer* sdCB = globals::state->sharedDataCB->CB();
				context->CSSetConstantBuffers(5, 1, &sdCB);
			}

			ID3D11ShaderResourceView* srvs[3] = { nullptr, nullptr, nullptr };
			auto depthSRV = Util::GetCurrentSceneDepthSRV(true);
			if (depthSRV) srvs[1] = depthSRV;

			auto bloomSRV = globals::features::bloom.GetBloomTextureSRV();
			if (bloomSRV) srvs[2] = bloomSRV;

			context->CSSetShaderResources(0, 3, srvs);

			ID3D11UnorderedAccessView* uavs[] = { outputUAV };
			context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

			uint32_t dispatchX = (width + 7) / 8;
			uint32_t dispatchY = (height + 7) / 8;
			context->Dispatch(dispatchX, dispatchY, 1);

			ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
			context->CSSetShaderResources(0, 3, nullSRVs);
		}
	}

	// 2. Dispatch native Vignette and Letterbox
	if (settings.EnableLetterbox || settings.EnableVignette) {
		auto nativeShader = GetPostProcessCS();
		if (nativeShader) {
			context->CSSetShader(nativeShader, nullptr, 0);

			PostProcessParams params{
				.letterboxHeight = settings.LetterboxHeight,
				.vignetteAmount = settings.VignetteAmount,
				.enableLetterbox = settings.EnableLetterbox ? 1.0f : 0.0f,
				.enableVignette = settings.EnableVignette ? 1.0f : 0.0f,
				.timer = globals::state ? globals::state->timer : 0.0f
			};
			postProcessParamsBuffer->Update(params);
			ID3D11Buffer* cb0 = postProcessParamsBuffer->CB();
			context->CSSetConstantBuffers(0, 1, &cb0);

			ID3D11UnorderedAccessView* uavs[] = { outputUAV };
			context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

			uint32_t dispatchX = (width + 7) / 8;
			uint32_t dispatchY = (height + 7) / 8;
			context->Dispatch(dispatchX, dispatchY, 1);
		}
	}

	// Cleanup
	context->CSSetShader(nullptr, nullptr, 0);
	ID3D11Buffer* nullCB = nullptr;
	context->CSSetConstantBuffers(0, 1, &nullCB);
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
}