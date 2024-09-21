#pragma once

#include <CoreMinimal.h>

struct Register8
{
public:
	Register8()
		: Byte(0)
	{}

	Register8(const Register8& _R)
		: Byte(_R.Byte)
	{}

	Register8(uint8_t Value)
		: Byte(Value)
	{}

	uint8_t Get()
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

protected:
	uint8_t Byte;
};

class Register16
{
public:
	Register16()
		: Word(0)
	{}

	Register16(const Register16& _R)
		: Word(_R.Word)
	{}

	Register16(uint16_t Value)
		: Word(Value)
	{}

	uint16_t Get()
	{
		return Word;
	}

	void Set(uint16_t Value)
	{
		Word = Value;
	}

	uint8_t GetHigh()
	{
		return L.Get();
	}

	uint8_t GetLow()
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

protected:
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
};
