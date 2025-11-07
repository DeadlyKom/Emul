#pragma once

#include <CoreMinimal.h>

struct FImageHandle
{
	FImageHandle(int32_t Index = INDEX_NONE)
		: ID(Index)
	{}

	FImageHandle& operator=(const FImageHandle& Other)
	{
		ID = Other.ID;
		return *this;
	}
	bool operator==(const FImageHandle& Other) const
	{
		return ID == Other.ID;
	}
	bool IsValid() const
	{
		return ID != INDEX_NONE;
	}

	int32_t ID;
};

struct FImage
{
	FImage(FImageHandle _Handle = INDEX_NONE, int32_t _Width = INDEX_NONE, int32_t _Height = INDEX_NONE)
		: Width((float)_Width)
		, Height((float)_Height)
		, Handle(_Handle)
		, ShaderResourceView(nullptr)
	{}

	FImage& operator=(const FImage& Other)
	{
		Size = Other.Size;
		Handle = Other.Handle;
		ShaderResourceView = Other.ShaderResourceView;
		return *this;
	}

	uint32_t GetLength() const { return (uint32_t)Size.x * (uint32_t)Size.y; }
	uint32_t GetFormatSize() const { return sizeof(uint32_t); }
	void Release()
	{
		if (ShaderResourceView != nullptr)
		{
			ShaderResourceView->Release();
			ShaderResourceView = nullptr;
		}
	}
	bool IsValid() const
	{
		return Handle.IsValid();
	}

	union
	{
		ImVec2 Size;
		struct
		{
			float Width;
			float Height;
		};
	};

	FImageHandle Handle;
	ID3D11ShaderResourceView* ShaderResourceView;
};

class FImageBase
{
public:
	static FImageBase& Get();
	static FImage& LoadImageFromResource(WORD ID, std::string Folder);
	static uint8_t* LoadToMemory(const std::filesystem::path& FilePath, int32_t& Width, int32_t& Height);
	static void ReleaseLoadedIntoMemory(uint8_t* ImageData);

	~FImageBase();
	void Initialize(ID3D11Device* _Device, ID3D11DeviceContext* _DeviceContext);

	FImage& LoadFromFile(const std::filesystem::path& FilePath);
	FImage& FromMemory(std::vector<uint8_t> Memory);
	FImage& CreateTexture(void* ImageData, int32_t Width, int32_t Height, UINT CPUAccessFlags = 0, D3D11_USAGE Usage = D3D11_USAGE::D3D11_USAGE_DEFAULT);
	bool UpdateTexture(FImageHandle _Handle, void* ImageData);
	FImage& GetImage(FImageHandle _Handle);

private:
	// low level (when releasing a resource, FreeToMemory must be called)
	bool ResizeRegion(const uint8_t* ImageData, const ImVec2& OriginalSize, const ImVec2& RequiredSize, uint8_t*& OutputImageData, const ImVec2& uv0, const ImVec2& uv1);
	void FreeToMemory(uint8_t* Data);
	bool Lock(ID3D11ShaderResourceView* ShaderResourceView, ID3D11Resource*& TextureResource, ID3D11Texture2D*& Texture, D3D11_MAPPED_SUBRESOURCE& MappedResource);
	void Unlock(ID3D11Resource*& TextureResource, ID3D11Texture2D*& Texture);

	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	std::unordered_map<int32_t, FImage> Images;
};
