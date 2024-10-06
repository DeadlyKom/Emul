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
	bool IsValid()
	{
		return ID != INDEX_NONE;
	}

	int32_t ID;
};

struct FImage
{
	FImage(FImageHandle _Handle, int32_t _Width = INDEX_NONE, int32_t _Height = INDEX_NONE)
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

	void* GetShaderResourceView() const { return ShaderResourceView; }
	void Release()
	{
		if (ShaderResourceView != nullptr)
		{
			ShaderResourceView->Release();
			ShaderResourceView = nullptr;
		}
	}
	bool IsValid()
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
	static bool FromResource(std::vector<uint8_t>& OutData, WORD ResourceID, std::string Folder = "");
	static FImageHandle LoadImageFromResource(WORD ID, std::string Folder);

	~FImageBase();
	void Initialize(ID3D11Device* _Device);

	FImageHandle LoadFromFile(const std::filesystem::path& FilePath);
	FImageHandle FromMemory(std::vector<uint8_t> Memory);
	FImage& GetImage(FImageHandle _Handle);

private:
	// low level (when releasing a resource, FreeToMemory must be called)
	uint8_t* LoadToMemory(const std::filesystem::path& FilePath, int32_t& Width, int32_t& Height);
	bool ResizeRegion(const uint8_t* ImageData, const ImVec2& OriginalSize, const ImVec2& RequiredSize, uint8_t*& OutputImageData, const ImVec2& uv0, const ImVec2& uv1);
	void FreeToMemory(uint8_t* Data);

	ID3D11Device* Device;
	std::unordered_map<int32_t, FImage> Images;
};
