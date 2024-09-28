#pragma once

#include <CoreMinimal.h>

struct Register8
{
	Register8()
		: Byte(0)
	{}

	Register8(const Register8& _R)
		: Byte(_R.Byte)
	{}

	Register8(uint8_t Value)
		: Byte(Value)
	{}

	uint8_t Get() const
	{
		return Byte;
	}

	void Set(uint8_t Value)
	{
		Byte = Value;
	}

	void Reset()
	{
		Byte = 0;
	}

	void AND(uint8_t Value)
	{
		Byte &= Value;
	}

	void OR(uint8_t Value)
	{
		Byte |= Value;
	}

	void XOR(uint8_t Value)
	{
		Byte ^= Value;
	}

	bool RL(bool Flag = false)
	{
		const uint8_t Mask = CHAR_BIT - 1;
		const bool Result = Byte & 0x80;
		Byte = (Byte << 1) | (Flag >> ((-1) & Mask));
		return Result;
	}
	
	bool RLC()
	{
		const uint8_t Mask = CHAR_BIT - 1;
		Byte = (Byte << 1) | (Byte >> ((-1) & Mask));
		return Byte & 0x01;
	}

	bool SLL(bool Flag = false)
	{
		const uint8_t Mask = CHAR_BIT - 1;
		const bool Result = Byte & 0x80;
		Byte = (Byte << 1) | (Flag >> ((-1) & Mask));
		return Result;
	}

	bool RR(bool Flag = false)
	{
		const uint8_t Mask = CHAR_BIT - 1;
		const bool Result = Byte & 0x01;
		Byte = (Byte >> 1) | (Flag << ((-1) & Mask));
		return Result;
	}
	
	bool RRC()
	{
		const uint8_t Mask = CHAR_BIT - 1;
		Byte = (Byte >> 1) | (Byte << ((-1) & Mask));
		return Byte & 0x80;
	}

	bool SRL(bool Flag = false)
	{
		const uint8_t Mask = CHAR_BIT - 1;
		const bool Result = Byte & 0x01;
		Byte = (Byte >> 1) | (Flag << ((-1) & Mask));
		return Result;
	}

	uint8_t RLD(uint8_t Value)
	{
		uint8_t Result = (Byte >> 4) & 0x0F;
		Byte = (Byte << 4) | (Value & 0x0F);
		Result |= Value & 0xF0;
		return Result;
	}

	uint8_t RRD(uint8_t Value)
	{
		uint8_t Result = Byte & 0x0F;
		Byte = (Byte >> 4) | ((Value & 0x0F) << 4);
		Result |= Value & 0xF0;
		return Result;
	}
	
	bool GetBit(uint8_t Bit)
	{
		return (Byte >> Bit) & 0x01;
	}

	void SetBit(uint8_t Bit)
	{
		Byte |= 1 << Bit;
	}

	void XorBit(uint8_t Bit)
	{
		Byte ^= 1 << Bit;
	}

	void ResetBit(uint8_t Bit)
	{
		Byte &= ~(1 << Bit);
	}

	void Increment()
	{
		Byte++;
	}

	void Decrement()
	{
		Byte--;
	}

	Register8& operator++()
	{
		return ++Byte, * this;
	}
	uint8_t operator*()
	{
		return Byte;
	}

	void operator=(const Register8& Other)
	{
		Byte = Other.Byte;
	}
	void operator+(const Register8& Other)
	{
		Byte += Other.Byte;
	}
	void operator+=(const Register8& Other)
	{
		Byte += Other.Byte;
	}
	void operator+(uint8_t Value)
	{
		Byte += Value;
	}
	void operator+=(uint8_t Value)
	{
		Byte += Value;
	}
	void operator-(const Register8& Other)
	{
		Byte -= Other.Byte;
	}
	void operator-=(const Register8& Other)
	{
		Byte -= Other.Byte;
	}
	void operator-(uint8_t Value)
	{
		Byte -= Value;
	}
	void operator-=(uint8_t Value)
	{
		Byte -= Value;
	}

	uint8_t Byte;
};

struct Register16
{
	Register16()
		: Word(0)
	{}

	Register16(const Register16& _R)
		: Word(_R.Word)
	{}

	Register16(uint16_t Value)
		: Word(Value)
	{}

	uint16_t Get() const
	{
		return Word;
	}

	void Set(uint16_t Value)
	{
		Word = Value;
	}

	uint8_t GetLow() const
	{
		return L.Get();
	}

	uint8_t GetHigh() const
	{
		return H.Get();
	}

	void Reset()
	{
		Word = 0;
	}

	void Increment()
	{
		Word++;
	}

	void Decrement()
	{
		Word--;
	}

	void Exchange(Register16& Other)
	{
		std::swap(Word, Other.Word);
	}

	Register16& operator++()
	{
		return ++Word, *this;
	}
	uint16_t operator*()
	{
		return Word;
	}

	void operator=(uint16_t Value)
	{
		Word = Value;
	}
	void operator=(const Register16& Other)
	{
		Word = Other.Word;
	}
	void operator+(const Register16& Other)
	{
		Word += Other.Word;
	}
	void operator+=(const Register16& Other)
	{
		Word += Other.Word;
	}
	void operator+(uint16_t Value)
	{
		Word += Value;
	}
	void operator+=(uint16_t Value)
	{
		Word += Value;
	}
	void operator-(const Register16& Other)
	{
		Word -= Other.Word;
	}
	void operator-=(const Register16& Other)
	{
		Word -= Other.Word;
	}
	void operator-(uint16_t Value)
	{
		Word -= Value;
	}
	void operator-=(uint16_t Value)
	{
		Word -= Value;
	}

	union
	{
		struct
		{
			Register8 L;
			Register8 H;
		};
		uint16_t Word;
	};
};

class RegisterIR : public Register16
{
public:
	uint8_t GetI() const
	{
		return GetHigh();
	}
	uint8_t GetR() const
	{
		return GetLow();
	}
	void IncrementR()
	{
		L.Set((GetR() & 0x80) | ((GetR() + 1) & 0x7F));
	}
	void operator=(uint16_t Value)
	{
		Word = Value;
	}
	void operator=(const RegisterIR& Other)
	{
		Word = Other.Word;
	}
};

enum class EZ80_Flag
{
	Carry			= 0,
	Add_Subtract	= 1,
	Parity_Overflow = 2,
	X				= 3, // undocumented bit 3
	Half_Carry		= 4,
	Y				= 5, // undocumented bit 5
	Zero			= 6,
	Sign			= 7,
};

class RegisterAF : public Register16
{
public:
	bool GetFlag(EZ80_Flag Flag)
	{
		return L.GetBit(static_cast<uint8_t>(Flag));
	}
	
	void SetFlag(EZ80_Flag Flag)
	{
		return L.SetBit(static_cast<uint8_t>(Flag));
	}
	
	void ResetFlag(EZ80_Flag Flag)
	{
		return L.ResetBit(static_cast<uint8_t>(Flag));
	}

	uint8_t GetAccumulator()
	{
		return H.Get();
	}

	void operator=(uint16_t Value)
	{
		Word = Value;
	}	
	void operator=(const RegisterAF& Other)
	{
		Word = Other.Word;
	}
};
