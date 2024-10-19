#pragma once

#include <CoreMinimal.h>
#include "Utils/Resource.h"

namespace Shader
{
	static inline ID3D11PixelShader* CreatePixelShaderFromResource(ID3D11Device* Device, WORD ResourceID)
	{
		ID3DBlob* ErrorBlob = nullptr;
		ID3DBlob* PixelShaderBlob = nullptr;
		ID3D11PixelShader* PixelShader = nullptr;

		std::vector<uint8_t> ShaderData;
		if (!Resource::Get(ShaderData, ResourceID, "SHADER"))
		{
			return nullptr;
		}

		if (FAILED(D3DCompile(ShaderData.data(), ShaderData.size(), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &PixelShaderBlob, &ErrorBlob)))
		{
			std::string Error((const char*)ErrorBlob->GetBufferPointer());
			LOG_ERROR("pixel shader failed. diagnostic:\n%s", Error.c_str());
			ErrorBlob->Release();
			return nullptr;
		}

		if (Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), NULL, &PixelShader) != S_OK)
		{
			PixelShaderBlob->Release();
			return nullptr;
		}
		PixelShaderBlob->Release();

		return PixelShader;
	}

	static inline void ReleasePixelShader(ID3D11PixelShader*& PixelShader)
	{
		if (!PixelShader)
		{
			return;
		}
		PixelShader->Release();
		PixelShader = nullptr;
	}

	template<typename T>
	static inline ID3D11Buffer* CreatePixelShaderConstantBuffer(ID3D11Device* Device)
	{
		ID3D11Buffer* ConstantBuffer = nullptr;

		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(T);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		Device->CreateBuffer(&desc, NULL, &ConstantBuffer);

		return ConstantBuffer;
	}

	static inline void ReleaseConstantBuffer(ID3D11Buffer*& ConstantBuffer)
	{
		if (!ConstantBuffer)
		{
			return;
		}
		ConstantBuffer->Release();
		ConstantBuffer = nullptr;
	}
}