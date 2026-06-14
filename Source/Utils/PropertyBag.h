#pragma once

#include <CoreMinimal.h>

enum class EPropertyType
{
    None,
    Int,
    Float,
    Bool,
    String,
    Struct
};

template<typename T>
const void* GetStructTypeId()
{
    static int32_t TypeId;
    return &TypeId;
}

struct IStructValue
{
    virtual ~IStructValue() = default;

    virtual const void* GetTypeId() const = 0;
    virtual const FName GetTypeName() const = 0;
    virtual std::unique_ptr<IStructValue> Clone() const = 0;
};

template<typename T>
struct TStructValue : public IStructValue
{
    T Value;
    TStructValue(const T& InValue)
        : Value(InValue)
    {}

    const void* GetTypeId() const override
    {
        return GetStructTypeId<T>();
    }

    const FName GetTypeName() const override
    {
        return T::StaticStructName();
    }

    std::unique_ptr<IStructValue> Clone() const override
    {
        return std::make_unique<TStructValue<T>>(Value);
    }
};

struct FPropertyDesc
{
    std::string Name;
    EPropertyType Type = EPropertyType::None;
};

struct FPropertyValue
{
    EPropertyType Type = EPropertyType::None;

    int32_t IntValue = 0;
    float FloatValue = 0.0f;
    bool BoolValue = false;
    std::string StringValue;

    std::unique_ptr<IStructValue> StructValue;

    FPropertyValue()
    {
    }

    FPropertyValue(const FPropertyValue& Other)
    {
        Type = Other.Type;

        IntValue = Other.IntValue;
        FloatValue = Other.FloatValue;
        BoolValue = Other.BoolValue;
        StringValue = Other.StringValue;

        if (Other.StructValue)
        {
            StructValue = Other.StructValue->Clone();
        }
    }

    FPropertyValue& operator=(const FPropertyValue& Other)
    {
        if (this == &Other)
        {
            return *this;
        }

        Type = Other.Type;

        IntValue = Other.IntValue;
        FloatValue = Other.FloatValue;
        BoolValue = Other.BoolValue;
        StringValue = Other.StringValue;

        if (Other.StructValue)
        {
            StructValue = Other.StructValue->Clone();
        }
        else
        {
            StructValue.reset();
        }

        return *this;
    }

    FPropertyValue(FPropertyValue&& Other) noexcept = default;
    FPropertyValue& operator=(FPropertyValue&& Other) noexcept = default;

    static FPropertyValue MakeInt(int32_t Value)
    {
        FPropertyValue Result;
        Result.Type = EPropertyType::Int;
        Result.IntValue = Value;
        return Result;
    }

    static FPropertyValue MakeFloat(float Value)
    {
        FPropertyValue Result;
        Result.Type = EPropertyType::Float;
        Result.FloatValue = Value;
        return Result;
    }

    static FPropertyValue MakeBool(bool Value)
    {
        FPropertyValue Result;
        Result.Type = EPropertyType::Bool;
        Result.BoolValue = Value;
        return Result;
    }

    static FPropertyValue MakeString(const std::string& Value)
    {
        FPropertyValue Result;
        Result.Type = EPropertyType::String;
        Result.StringValue = Value;
        return Result;
    }

    template<typename T>
    static FPropertyValue MakeStruct(const T& Value)
    {
        FPropertyValue Result;
        Result.Type = EPropertyType::Struct;
        Result.StructValue = std::make_unique<TStructValue<T>>(Value);
        return Result;
    }
};

class FPropertyBag
{
public:
    FPropertyBag()
        : bIsValid(false)
    {}

    static const FPropertyBag Invalid;

    bool AddInt(const std::string& Name, int32_t Value)
    {
        return AddProperty(Name, FPropertyValue::MakeInt(Value));
    }

    bool AddFloat(const std::string& Name, float Value)
    {
        return AddProperty(Name, FPropertyValue::MakeFloat(Value));
    }

    bool AddBool(const std::string& Name, bool Value)
    {
        return AddProperty(Name, FPropertyValue::MakeBool(Value));
    }

    bool AddString(const std::string& Name, const std::string& Value)
    {
        return AddProperty(Name, FPropertyValue::MakeString(Value));
    }

    template<typename T>
    bool AddStruct(const std::string& Name, const T& Value)
    {
        return AddProperty(Name, FPropertyValue::MakeStruct<T>(Value));
    }

    bool GetInt(const std::string& Name, int32_t& OutValue) const
    {
        int32_t Index = FindPropertyIndex(Name);

        if (Index < 0)
        {
            return false;
        }

        if (Values[Index].Type != EPropertyType::Int)
        {
            return false;
        }

        OutValue = Values[Index].IntValue;
        return true;
    }

    bool GetFloat(const std::string& Name, float& OutValue) const
    {
        int32_t Index = FindPropertyIndex(Name);

        if (Index < 0)
        {
            return false;
        }

        if (Values[Index].Type != EPropertyType::Float)
        {
            return false;
        }

        OutValue = Values[Index].FloatValue;
        return true;
    }

    bool GetBool(const std::string& Name, bool& OutValue) const
    {
        int32_t Index = FindPropertyIndex(Name);

        if (Index < 0)
        {
            return false;
        }

        if (Values[Index].Type != EPropertyType::Bool)
        {
            return false;
        }

        OutValue = Values[Index].BoolValue;
        return true;
    }

    bool GetString(const std::string& Name, std::string& OutValue) const
    {
        int32_t Index = FindPropertyIndex(Name);

        if (Index < 0)
        {
            return false;
        }

        if (Values[Index].Type != EPropertyType::String)
        {
            return false;
        }

        OutValue = Values[Index].StringValue;
        return true;
    }

    template<typename T>
    bool GetStruct(const std::string& Name, T& OutValue) const
    {
        int32_t Index = FindPropertyIndex(Name);
        if (Index < 0)
        {
            return false;
        }

        const FPropertyValue& PropertyValue = Values[Index];
        if (PropertyValue.Type != EPropertyType::Struct)
        {
            return false;
        }

        if (!PropertyValue.StructValue)
        {
            return false;
        }

        if (PropertyValue.StructValue->GetTypeId() != GetStructTypeId<T>())
        {
            return false;
        }

        const TStructValue<T>* TypedValue = static_cast<const TStructValue<T>*>(PropertyValue.StructValue.get());

        OutValue = TypedValue->Value;
        return true;
    }

    template<typename T>
    bool SetStruct(const std::string& Name, const T& NewValue)
    {
        int32_t Index = FindPropertyIndex(Name);

        if (Index < 0)
        {
            return false;
        }

        FPropertyValue& PropertyValue = Values[Index];

        if (PropertyValue.Type != EPropertyType::Struct)
        {
            return false;
        }

        if (!PropertyValue.StructValue)
        {
            return false;
        }

        if (PropertyValue.StructValue->GetTypeId() != GetStructTypeId<T>())
        {
            return false;
        }

        TStructValue<T>* TypedValue = static_cast<TStructValue<T>*>(PropertyValue.StructValue.get());

        TypedValue->Value = NewValue;
        return true;
    }

    bool HasProperty(const std::string& Name) const
    {
        return FindPropertyIndex(Name) >= 0;
    }

    bool RemoveProperty(const std::string& Name)
    {
        const int32_t Index = FindPropertyIndex(Name);
        if (Index < 0)
        {
            return false;
        }

        Descriptions.erase(Descriptions.begin() + Index);
        Values.erase(Values.begin() + Index);
        bIsValid = !Descriptions.empty();
        return true;
    }

    int32_t GetNum() const
    {
        return (int32_t)Descriptions.size();
    }

    const FPropertyDesc& GetDesc(int32_t Index) const
    {
        return Descriptions[Index];
    }

    const FPropertyValue& GetValue(int32_t Index) const
    {
        return Values[Index];
    }

    bool IsValid() const
    {
        if (!bIsValid)
        {
            return false;
        }

        if (Descriptions.size() != Values.size())
        {
            return false;
        }

        for (int32_t Index = 0; Index < (int32_t)Descriptions.size(); Index++)
        {
            const FPropertyDesc& Desc = Descriptions[Index];
            const FPropertyValue& Value = Values[Index];

            if (Desc.Name.empty())
            {
                return false;
            }

            if (Desc.Type == EPropertyType::None)
            {
                return false;
            }

            if (Desc.Type != Value.Type)
            {
                return false;
            }

            if (Value.Type == EPropertyType::Struct)
            {
                if (!Value.StructValue)
                {
                    return false;
                }
            }
            else
            {
                if (Value.StructValue)
                {
                    return false;
                }
            }

            for (int32_t OtherIndex = Index + 1; OtherIndex < (int32_t)Descriptions.size(); OtherIndex++)
            {
                if (Descriptions[OtherIndex].Name == Desc.Name)
                {
                    return false;
                }
            }
        }

        return true;
    }
    
    void Reset()
    {
        Descriptions.clear();
        Values.clear();
        bIsValid = false;
    }

private:
    bool AddProperty(const std::string& Name, const FPropertyValue& Value)
    {
        if (FindPropertyIndex(Name) >= 0)
        {
            return false;
        }

        FPropertyDesc Desc;
        Desc.Name = Name;
        Desc.Type = Value.Type;

        Descriptions.push_back(Desc);
        Values.push_back(Value);

        bIsValid = true;
        return true;
    }
    int32_t FindPropertyIndex(const std::string& Name) const
    {
        for (int32_t Index = 0; Index < (int32_t)Descriptions.size(); Index++)
        {
            if (Descriptions[Index].Name == Name)
            {
                return Index;
            }
        }
        return INDEX_NONE;
    }

    bool bIsValid;
    std::vector<FPropertyDesc> Descriptions;
    std::vector<FPropertyValue> Values;
};
