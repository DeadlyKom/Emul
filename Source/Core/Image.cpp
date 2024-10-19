#include "Image.h"
#include "AppFramework.h"
#include "Utils/Resource.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb\stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_DEFAULT_FILTER_UPSAMPLE    STBIR_FILTER_BOX
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE  STBIR_FILTER_BOX
#include "stb\stb_image_resize.h"

namespace
{
	static std::atomic_int Counter = 0;
	static std::mutex ImageMutex;
	static FImage DefaultImage(INDEX_NONE);
}

FImageBase& FImageBase::Get()
{
	static std::shared_ptr<FImageBase> Instance(new FImageBase);
	return *Instance.get();
}

FImageHandle FImageBase::LoadImageFromResource(WORD ID, std::string Folder)
{
	std::vector<uint8_t> ImageData;
	if (Resource::Get(ImageData, ID, Folder))
	{
		return FImageBase::Get().FromMemory(ImageData);
	}

	return FImageHandle();
}

FImageBase::~FImageBase()
{
}

void FImageBase::Initialize(ID3D11Device* _Device, ID3D11DeviceContext* _DeviceContext)
{
	Device = _Device;
	DeviceContext = _DeviceContext;
}

FImageHandle FImageBase::LoadFromFile(const std::filesystem::path& FilePath)
{
	int32_t Width, Height;

	ImageMutex.lock();
	uint8_t* ImageData = LoadToMemory(FilePath, Width, Height);
	if (ImageData == nullptr || Device == nullptr)
	{
		return FImageHandle();
	}

	FImage Image(FImageHandle(Counter), Width, Height);

	// create texture
	D3D11_TEXTURE2D_DESC Texture2D;
	ZeroMemory(&Texture2D, sizeof(Texture2D));
	Texture2D.Width = Width;
	Texture2D.Height = Height;
	Texture2D.MipLevels = 1;
	Texture2D.ArraySize = 1;
	Texture2D.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Texture2D.SampleDesc.Count = 1;
	Texture2D.Usage = D3D11_USAGE_DEFAULT;
	Texture2D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Texture2D.CPUAccessFlags = 0;

	ID3D11Texture2D* Texture = nullptr;
	D3D11_SUBRESOURCE_DATA SubResource;
	SubResource.pSysMem = ImageData;
	SubResource.SysMemPitch = Width * 4;
	SubResource.SysMemSlicePitch = 0;
	Device->CreateTexture2D(&Texture2D, &SubResource, &Texture);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVD;
	ZeroMemory(&SRVD, sizeof(SRVD));
	SRVD.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SRVD.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVD.Texture2D.MipLevels = Texture2D.MipLevels;
	SRVD.Texture2D.MostDetailedMip = 0;
	Device->CreateShaderResourceView(Texture, &SRVD, &Image.ShaderResourceView);
	Texture->Release();

	stbi_image_free(ImageData);
	Images.emplace(Counter++, Image);

	ImageMutex.unlock();
	return Image.Handle;
}

FImageHandle FImageBase::FromMemory(std::vector<uint8_t> Memory)
{
	int32_t Width, Height;

	ImageMutex.lock();
	uint8_t* ImageData = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(Memory.data()), (int32_t)Memory.size(), &Width, &Height, NULL, 4);
	if (ImageData == nullptr || Device == nullptr)
	{
		return FImageHandle();
	}

	FImage Image(FImageHandle(Counter), Width, Height);

	// create texture
	D3D11_TEXTURE2D_DESC Texture2D;
	ZeroMemory(&Texture2D, sizeof(Texture2D));
	Texture2D.Width = Width;
	Texture2D.Height = Height;
	Texture2D.MipLevels = 1;
	Texture2D.ArraySize = 1;
	Texture2D.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Texture2D.SampleDesc.Count = 1;
	Texture2D.Usage = D3D11_USAGE_DEFAULT;
	Texture2D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Texture2D.CPUAccessFlags = 0;

	ID3D11Texture2D* Texture = nullptr;
	D3D11_SUBRESOURCE_DATA SubResource;
	SubResource.pSysMem = ImageData;
	SubResource.SysMemPitch = Width * 4;
	SubResource.SysMemSlicePitch = 0;
	Device->CreateTexture2D(&Texture2D, &SubResource, &Texture);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVD;
	ZeroMemory(&SRVD, sizeof(SRVD));
	SRVD.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SRVD.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVD.Texture2D.MipLevels = Texture2D.MipLevels;
	SRVD.Texture2D.MostDetailedMip = 0;
	Device->CreateShaderResourceView(Texture, &SRVD, &Image.ShaderResourceView);
	Texture->Release();

	stbi_image_free(ImageData);
	Images.emplace(Counter++, Image);

	ImageMutex.unlock();
	return Image.Handle;
}

FImage& FImageBase::CreateTexture(void* ImageData, int32_t Width, int32_t Height, UINT CPUAccessFlags /*= 0*/, D3D11_USAGE Usage /*= D3D11_USAGE::D3D11_USAGE_DEFAULT*/)
{
	ImageMutex.lock();
	FImage Image(FImageHandle(Counter), Width, Height);

	// create texture
	D3D11_TEXTURE2D_DESC Texture2D;
	ZeroMemory(&Texture2D, sizeof(Texture2D));
	Texture2D.Width = (UINT)Width;
	Texture2D.Height = (UINT)Height;
	Texture2D.MipLevels = 1;
	Texture2D.ArraySize = 1;
	Texture2D.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Texture2D.SampleDesc.Count = 1;
	Texture2D.Usage = Usage;
	Texture2D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	Texture2D.CPUAccessFlags = (UINT)CPUAccessFlags;

	ID3D11Texture2D* Texture = nullptr;
	D3D11_SUBRESOURCE_DATA SubResource;
	SubResource.pSysMem = ImageData;
	SubResource.SysMemPitch = (UINT)Width * 4;
	SubResource.SysMemSlicePitch = 0;
	Device->CreateTexture2D(&Texture2D, &SubResource, &Texture);

	// create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVD;
	ZeroMemory(&SRVD, sizeof(SRVD));
	SRVD.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SRVD.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVD.Texture2D.MipLevels = Texture2D.MipLevels;
	SRVD.Texture2D.MostDetailedMip = 0;
	Device->CreateShaderResourceView(Texture, &SRVD, &Image.ShaderResourceView);
	Texture->Release();

	Images.emplace(Counter++, Image);
	ImageMutex.unlock();

	return GetImage(Image.Handle);
}

void FImageBase::UpdateTexture(FImageHandle _Handle, void* ImageData)
{
	FImage& Image = GetImage(_Handle);

	ID3D11Texture2D* Texture = nullptr;
	ID3D11Resource* TextureResource = nullptr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	if (Lock(Image.ShaderResourceView, TextureResource, Texture, MappedResource))
	{
		memcpy(MappedResource.pData, ImageData, Image.GetLength() * Image.GetFormatSize());
		Unlock(TextureResource, Texture);
	}
	return;
}

FImage& FImageBase::GetImage(FImageHandle _Handle)
{
	for (auto& [Handle, Image] : Images)
	{
		if (Handle == _Handle)
		{
			return Image;
		}
	}
	return DefaultImage;
}

uint8_t* FImageBase::LoadToMemory(const std::filesystem::path& FilePath, int32_t& Width, int32_t& Height)
{
	return stbi_load(FilePath.string().c_str(), &Width, &Height, NULL, 4);
}

bool FImageBase::ResizeRegion(const uint8_t* ImageData, const ImVec2& OriginalSize, const ImVec2& RequiredSize, uint8_t*& OutputImageData, const ImVec2& uv0, const ImVec2& uv1)
{
	if (stbir_resize_region((unsigned char*)ImageData,			(int32_t)OriginalSize.x, (int32_t)OriginalSize.y, 0,
							(unsigned char*)OutputImageData,	(int32_t)RequiredSize.x, (int32_t)RequiredSize.y, 0,
							STBIR_TYPE_UINT8,
							4, 3, 0,
							STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP,
							STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT,
							STBIR_COLORSPACE_SRGB,
							nullptr,
							uv0.x, uv0.y, uv1.x, uv1.y) == 0)
	{
		LOG("issue resizing region texture");

		assert(false);
		delete[] ImageData;
		return false;
	}
	return true;
}

void FImageBase::FreeToMemory(uint8_t* Data)
{
	stbi_image_free(Data);
}

bool FImageBase::Lock(ID3D11ShaderResourceView* ShaderResourceView, ID3D11Resource*& TextureResource, ID3D11Texture2D*& Texture, D3D11_MAPPED_SUBRESOURCE& MappedResource)
{
	if (ShaderResourceView == nullptr)
	{
		return false;
	}

	ShaderResourceView->GetResource(&TextureResource);
	if (FAILED(TextureResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&Texture))))
	{
		return false;
	}

	return DeviceContext->Map(Texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource) == S_OK;
}

void FImageBase::Unlock(ID3D11Resource*& TextureResource, ID3D11Texture2D*& Texture)
{
	if (Texture != nullptr)
	{
		DeviceContext->Unmap(Texture, 0);
		Texture->Release();
	}

	if (TextureResource != nullptr)
	{
		TextureResource->Release();
	}
}
