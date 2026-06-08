#include "PostProcessOverlay.h"
#include "Deferred.h"
#include "Globals.h"
#include "State.h"
#include <imgui.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PostProcessOverlay::Settings,
	EnableLetterbox,
	LetterboxHeight,
	EnableVignette,
	VignetteAmount,
	EnablePostpass,
	EnableUnderwaterDistortion,
	UnderwaterDistortionStrength,
	UnderwaterDistortionSpeed)

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

	ImGui::Checkbox("Enable Underwater Distortion", &settings.EnableUnderwaterDistortion);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Applies a wavy screen-space UV distortion (mirroring ENB's enbunderwater.fx)\nwhile the camera is below the water surface.");
	if (settings.EnableUnderwaterDistortion) {
		ImGui::SliderFloat("Underwater Distortion Strength", &settings.UnderwaterDistortionStrength, 0.0f, 5.0f);
		ImGui::SliderFloat("Underwater Distortion Speed", &settings.UnderwaterDistortionSpeed, 0.0f, 5.0f);
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
	underwaterParamsBuffer = new ConstantBuffer(ConstantBufferDesc<UnderwaterParams>());
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
	if (underwaterDistortionCS) {
		underwaterDistortionCS->Release();
		underwaterDistortionCS = nullptr;
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

ID3D11ComputeShader* PostProcessOverlay::GetUnderwaterDistortionCS()
{
	if (!underwaterDistortionCS) {
		logger::debug("Compiling UnderwaterDistortionCS");
		underwaterDistortionCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\PostProcessOverlay\\UnderwaterDistortionCS.hlsl", {}, "cs_5_0"));
	}
	return underwaterDistortionCS;
}

ID3D11ShaderResourceView* PostProcessOverlay::GetFrameCopySRV(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height)
{
	auto context = globals::d3d::context;

	if (!frameCopyTexture || frameCopyTexture->desc.Width != width || frameCopyTexture->desc.Height != height) {
		D3D11_UNORDERED_ACCESS_VIEW_DESC outputUavDesc;
		outputUAV->GetDesc(&outputUavDesc);

		if (frameCopyTexture) {
			delete frameCopyTexture;
			frameCopyTexture = nullptr;
		}

		D3D11_TEXTURE2D_DESC desc{
			.Width = width,
			.Height = height,
			.MipLevels = 1,
			.ArraySize = 1,
			.Format = outputUavDesc.Format,
			.SampleDesc = { .Count = 1 },
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_SHADER_RESOURCE
		};

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.Format = desc.Format,
			.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
			.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1 }
		};

		frameCopyTexture = new Texture2D(desc, "PostProcessOverlay::FrameCopy");
		frameCopyTexture->CreateSRV(srvDesc);
	}

	winrt::com_ptr<ID3D11Resource> outputResource;
	outputUAV->GetResource(outputResource.put());
	context->CopyResource(frameCopyTexture->resource.get(), outputResource.get());

	return frameCopyTexture->srv.get();
}

void PostProcessOverlay::RenderUnderwaterDistortion(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height)
{
	if (!settings.EnableUnderwaterDistortion)
		return;

	auto player = globals::game::player;
	if (!player || !player->IsInWater())
		return;

	auto shader = GetUnderwaterDistortionCS();
	if (!shader)
		return;

	auto context = globals::d3d::context;

	// The distortion shader needs to sample the pre-distortion frame while
	// writing the warped result back to the same resource, so it samples from
	// a filtered copy to avoid a read/write hazard on outputUAV.
	ID3D11ShaderResourceView* srv = GetFrameCopySRV(outputUAV, width, height);

	context->CSSetShader(shader, nullptr, 0);

	UnderwaterParams params{
		.strength = settings.UnderwaterDistortionStrength,
		.speed = settings.UnderwaterDistortionSpeed,
		.timer = globals::state ? globals::state->timer : 0.0f
	};
	underwaterParamsBuffer->Update(params);
	ID3D11Buffer* cb0 = underwaterParamsBuffer->CB();
	context->CSSetConstantBuffers(0, 1, &cb0);

	ID3D11SamplerState* sampler = Deferred::GetSingleton()->linearSampler;
	context->CSSetSamplers(0, 1, &sampler);

	context->CSSetShaderResources(0, 1, &srv);

	ID3D11UnorderedAccessView* uavs[] = { outputUAV };
	context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	uint32_t dispatchX = (width + 7) / 8;
	uint32_t dispatchY = (height + 7) / 8;
	context->Dispatch(dispatchX, dispatchY, 1);

	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->CSSetShaderResources(0, 1, &nullSRV);
	ID3D11SamplerState* nullSampler = nullptr;
	context->CSSetSamplers(0, 1, &nullSampler);
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	context->CSSetShader(nullptr, nullptr, 0);
}

void PostProcessOverlay::Present(ID3D11UnorderedAccessView* outputUAV, uint32_t width, uint32_t height)
{
	if (!settings.EnableLetterbox && !settings.EnableVignette && !settings.EnablePostpass && !settings.EnableUnderwaterDistortion)
		return;

	auto context = globals::d3d::context;

	// 0. Apply underwater screen-space distortion first so later overlays
	// (vignette, letterbox, postpass) composite on top of the warped frame.
	RenderUnderwaterDistortion(outputUAV, width, height);

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

			// t0: filtered copy of the frame as it stands entering this pass --
			// lets the user shader safely do neighborhood/UV-offset sampling
			// (chromatic aberration, blur/distortion, bokeh-style gathers, etc)
			// via SampleLevel(LinearSampler, ...) without an in-place
			// read/write hazard on outputUAV, which it must still write the
			// final composited result back to.
			ID3D11ShaderResourceView* srvs[3] = { GetFrameCopySRV(outputUAV, width, height), nullptr, nullptr };
			auto depthSRV = Util::GetCurrentSceneDepthSRV(true);
			if (depthSRV) srvs[1] = depthSRV;

			auto bloomSRV = globals::features::bloom.GetBloomTextureSRV();
			if (bloomSRV) srvs[2] = bloomSRV;

			context->CSSetShaderResources(0, 3, srvs);

			ID3D11SamplerState* sampler = Deferred::GetSingleton()->linearSampler;
			context->CSSetSamplers(0, 1, &sampler);

			ID3D11UnorderedAccessView* uavs[] = { outputUAV };
			context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

			uint32_t dispatchX = (width + 7) / 8;
			uint32_t dispatchY = (height + 7) / 8;
			context->Dispatch(dispatchX, dispatchY, 1);

			ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr };
			context->CSSetShaderResources(0, 3, nullSRVs);
			ID3D11SamplerState* nullSampler = nullptr;
			context->CSSetSamplers(0, 1, &nullSampler);
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