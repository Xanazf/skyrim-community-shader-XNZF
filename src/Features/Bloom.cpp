#include "Bloom.h"
#include "Deferred.h"
#include "Globals.h"
#include <imgui.h>
#include <DDSTextureLoader.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	Bloom::Settings,
	EnableBloom,
	BloomThreshold,
	BloomIntensity,
	EnableLens,
	GlareIntensity,
	DirtIntensity)

void Bloom::RestoreDefaultSettings()
{
	settings = {};
}

void Bloom::LoadSettings(json& o_json)
{
	settings = o_json;
}

void Bloom::SaveSettings(json& o_json)
{
	o_json = settings;
}

void Bloom::DrawSettings()
{
	if (ImGui::TreeNodeEx("Bloom & Lens", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enable Bloom", &settings.EnableBloom);
		if (settings.EnableBloom) {
			ImGui::SliderFloat("Threshold", &settings.BloomThreshold, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("Intensity", &settings.BloomIntensity, 0.0f, 5.0f, "%.2f");

			ImGui::Spacing();
			ImGui::Checkbox("Enable Lens FX", &settings.EnableLens);
			if (settings.EnableLens) {
				ImGui::SliderFloat("Glare Intensity", &settings.GlareIntensity, 0.0f, 5.0f, "%.2f");
				ImGui::SliderFloat("Dirt Intensity", &settings.DirtIntensity, 0.0f, 5.0f, "%.2f");
			}
		}
		ImGui::TreePop();
	}
}

void Bloom::SetupResources()
{
	bloomParamsBuffer = new ConstantBuffer(ConstantBufferDesc<BloomParams>());

	auto device = globals::d3d::device;
	auto context = globals::d3d::context;

	// Load lensmask.dds if it exists
	if (std::filesystem::exists("Data\\Shaders\\Bloom\\lensmask.dds")) {
		DirectX::CreateDDSTextureFromFile(device, context, L"Data\\Shaders\\Bloom\\lensmask.dds", nullptr, lensDirtView.put());
	} else {
		logger::warn("lensmask.dds not found in Data\\Shaders\\Bloom\\");
	}
}

void Bloom::ClearShaderCache()
{
	if (bloomCS)
		bloomCS->Release();
	bloomCS = nullptr;
}

ID3D11ComputeShader* Bloom::GetBloomCS()
{
	if (!bloomCS) {
		logger::debug("Compiling BloomCS");
		bloomCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\Bloom\\BloomCS.hlsl", {}, "cs_5_0"));
	}
	return bloomCS;
}

void Bloom::RenderBloom(ID3D11ShaderResourceView* preTonemapSRV, uint32_t width, uint32_t height)
{
	if (!settings.EnableBloom)
		return;

	auto context = globals::d3d::context;

	// 1. Dynamic viewport resizing/allocation
	if (!bloomDownsampleTextures[0] || bloomDownsampleTextures[0]->desc.Width != width / 2 || bloomDownsampleTextures[0]->desc.Height != height / 2) {
		uint32_t w = width;
		uint32_t h = height;
		for (uint32_t i = 0; i < MIP_COUNT; ++i) {
			if (bloomDownsampleTextures[i]) delete bloomDownsampleTextures[i];
			if (bloomUpsampleTextures[i]) delete bloomUpsampleTextures[i];

			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);

			D3D11_TEXTURE2D_DESC desc{
				.Width = w,
				.Height = h,
				.MipLevels = 1,
				.ArraySize = 1,
				.Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
				.SampleDesc = { .Count = 1 },
				.Usage = D3D11_USAGE_DEFAULT,
				.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS
			};

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
				.Format = desc.Format,
				.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
				.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1 }
			};

			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{
				.Format = desc.Format,
				.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
				.Texture2D = { .MipSlice = 0 }
			};

			bloomDownsampleTextures[i] = new Texture2D(desc, std::format("Bloom::DownsampleMip{}", i).c_str());
			bloomDownsampleTextures[i]->CreateSRV(srvDesc);
			bloomDownsampleTextures[i]->CreateUAV(uavDesc);

			bloomUpsampleTextures[i] = new Texture2D(desc, std::format("Bloom::UpsampleMip{}", i).c_str());
			bloomUpsampleTextures[i]->CreateSRV(srvDesc);
			bloomUpsampleTextures[i]->CreateUAV(uavDesc);
		}
	}

	auto shader = GetBloomCS();
	if (!shader)
		return;

	context->CSSetShader(shader, nullptr, 0);

	ID3D11SamplerState* sampler = Deferred::GetSingleton()->linearSampler;
	context->CSSetSamplers(0, 1, &sampler);

	// Pass 0: Highlight Extract & Downsample
	{
		BloomParams params{
			.threshold = settings.BloomThreshold,
			.intensity = settings.BloomIntensity,
			.glareIntensity = settings.GlareIntensity,
			.dirtIntensity = settings.DirtIntensity,
			.passType = 0
		};
		bloomParamsBuffer->Update(params);
		ID3D11Buffer* cb = bloomParamsBuffer->CB();
		context->CSSetConstantBuffers(0, 1, &cb);

		ID3D11ShaderResourceView* srvs[] = { preTonemapSRV };
		context->CSSetShaderResources(0, 1, srvs);

		ID3D11UnorderedAccessView* uavs[] = { bloomDownsampleTextures[0]->uav.get() };
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

		uint32_t dispatchX = (bloomDownsampleTextures[0]->desc.Width + 7) / 8;
		uint32_t dispatchY = (bloomDownsampleTextures[0]->desc.Height + 7) / 8;
		context->Dispatch(dispatchX, dispatchY, 1);

		// Unbind
		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->CSSetShaderResources(0, 1, &nullSRV);
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}

	// Pass 1: Downsampling loop
	for (uint32_t i = 1; i < MIP_COUNT; ++i) {
		BloomParams params{
			.threshold = settings.BloomThreshold,
			.intensity = settings.BloomIntensity,
			.glareIntensity = settings.GlareIntensity,
			.dirtIntensity = settings.DirtIntensity,
			.passType = 1
		};
		bloomParamsBuffer->Update(params);

		ID3D11ShaderResourceView* srvs[] = { bloomDownsampleTextures[i - 1]->srv.get() };
		context->CSSetShaderResources(0, 1, srvs);

		ID3D11UnorderedAccessView* uavs[] = { bloomDownsampleTextures[i]->uav.get() };
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

		uint32_t dispatchX = (bloomDownsampleTextures[i]->desc.Width + 7) / 8;
		uint32_t dispatchY = (bloomDownsampleTextures[i]->desc.Height + 7) / 8;
		context->Dispatch(dispatchX, dispatchY, 1);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->CSSetShaderResources(0, 1, &nullSRV);
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}

	// Copy the smallest mip to upsample buffer to bootstrap
	context->CopyResource(bloomUpsampleTextures[3]->resource.get(), bloomDownsampleTextures[3]->resource.get());

	// Pass 2: Upsampling loop
	for (int32_t i = 2; i >= 0; --i) {
		BloomParams params{
			.threshold = settings.BloomThreshold,
			.intensity = settings.BloomIntensity,
			.glareIntensity = settings.GlareIntensity,
			.dirtIntensity = settings.DirtIntensity,
			.passType = 2
		};
		bloomParamsBuffer->Update(params);

		ID3D11ShaderResourceView* srvs[] = {
			bloomUpsampleTextures[i + 1]->srv.get(),
			bloomDownsampleTextures[i]->srv.get()
		};
		context->CSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[] = { bloomUpsampleTextures[i]->uav.get() };
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

		uint32_t dispatchX = (bloomUpsampleTextures[i]->desc.Width + 7) / 8;
		uint32_t dispatchY = (bloomUpsampleTextures[i]->desc.Height + 7) / 8;
		context->Dispatch(dispatchX, dispatchY, 1);

		ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
		context->CSSetShaderResources(0, 2, nullSRVs);
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
	}

	// Pass 3: Lens FX (Starburst Glare and Lens Dirt) if enabled
	if (settings.EnableLens && lensDirtView) {
		BloomParams params{
			.threshold = settings.BloomThreshold,
			.intensity = settings.BloomIntensity,
			.glareIntensity = settings.GlareIntensity,
			.dirtIntensity = settings.DirtIntensity,
			.passType = 3
		};
		bloomParamsBuffer->Update(params);

		ID3D11ShaderResourceView* srvs[] = {
			bloomUpsampleTextures[0]->srv.get(),
			lensDirtView.get()
		};
		context->CSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[] = { bloomDownsampleTextures[0]->uav.get() };
		context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

		uint32_t dispatchX = (bloomDownsampleTextures[0]->desc.Width + 7) / 8;
		uint32_t dispatchY = (bloomDownsampleTextures[0]->desc.Height + 7) / 8;
		context->Dispatch(dispatchX, dispatchY, 1);

		ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
		context->CSSetShaderResources(0, 2, nullSRVs);
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

		// Copy back to bloomUpsampleTextures[0] so it's the final texture
		context->CopyResource(bloomUpsampleTextures[0]->resource.get(), bloomDownsampleTextures[0]->resource.get());
	}

	// Cleanup compute pipeline state
	context->CSSetShader(nullptr, nullptr, 0);
	ID3D11Buffer* nullCB = nullptr;
	context->CSSetConstantBuffers(0, 1, &nullCB);
	ID3D11SamplerState* nullSampler = nullptr;
	context->CSSetSamplers(0, 1, &nullSampler);
}
