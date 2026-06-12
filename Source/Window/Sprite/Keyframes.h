#pragma once

#include <CoreMinimal.h>
#include <Utils/PropertyBag.h>

namespace FPropertyTag
{
	inline constexpr const char* LimitArea = "Limit Area";
}

struct FTilemapCellData_Rect
{
	ImRect Rect;
	static const FName StaticStructName() { return TEXT("FTilemapCellData_Rect"); }
	FTilemapCellData_Rect(const ImRect& _Rect = {})
		: Rect(_Rect)
	{}
};

struct FKeyframes
{
	FKeyframes()
		: MaxFrames(INDEX_NONE)
		, MaxLayers(INDEX_NONE)
	{}

	bool IsValid() const
	{
		if (MaxFrames <= 0)
		{
			return false;
		}

		if (MaxLayers <= 0)
		{
			return false;
		}

		if ((int32_t)Property.size() != MaxFrames * MaxLayers)
		{
			return false;
		}

		return true;
	}

	void Reset()
	{
		MaxFrames = INDEX_NONE;
		MaxLayers = INDEX_NONE;
		Property.clear();
	}

	bool Make(int32_t _MaxFrames, int32_t _MaxLayers)
	{
		Reset();

		if (_MaxFrames <= 0)
		{
			return false;
		}

		if (_MaxLayers <= 0)
		{
			return false;
		}

		MaxFrames = _MaxFrames;
		MaxLayers = _MaxLayers;

		Property.resize(MaxFrames * MaxLayers);
		return true;
	}

	bool IsFrameValid(int32_t Frame) const
	{
		if (Frame < 0)
		{
			return false;
		}

		if (Frame >= MaxFrames)
		{
			return false;
		}

		return true;
	}

	bool IsLayerValid(int32_t Layer) const
	{
		if (Layer < 0)
		{
			return false;
		}

		if (Layer >= MaxLayers)
		{
			return false;
		}

		return true;
	}

	int32_t GetIndex(int32_t Frame, int32_t Layer) const
	{
		if (!IsValid())
		{
			return INDEX_NONE;
		}

		if (!IsFrameValid(Frame))
		{
			return INDEX_NONE;
		}

		if (!IsLayerValid(Layer))
		{
			return INDEX_NONE;
		}

		return Layer * MaxFrames + Frame;
	}

	bool IsIndexValid(int32_t Index) const
	{
		if (!IsValid())
		{
			return false;
		}

		if (Index < 0)
		{
			return false;
		}

		if (Index >= (int32_t)Property.size())
		{
			return false;
		}

		return true;
	}

	const FPropertyBag& GetProperty(int32_t Frame, int32_t Layer) const
	{
		const int32_t Index = GetIndex(Frame, Layer);

		if (!IsIndexValid(Index))
		{
			return FPropertyBag::Invalid;
		}

		return Property[Index];
	}

	FPropertyBag* GetMutableProperty(int32_t Frame, int32_t Layer)
	{
		const int32_t Index = GetIndex(Frame, Layer);

		if (!IsIndexValid(Index))
		{
			return nullptr;
		}

		return &Property[Index];
	}

	bool SetProperty(int32_t Frame, int32_t Layer, const FPropertyBag& NewProperty)
	{
		FPropertyBag* Bag = GetMutableProperty(Frame, Layer);
		if (Bag == nullptr)
		{
			return false;
		}

		*Bag = NewProperty;
		return true;
	}

private:
	int32_t MaxFrames;
	int32_t MaxLayers;
	std::vector<FPropertyBag> Property;
};
