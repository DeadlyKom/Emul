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

	Register8& operator++()
	{
		return ++Byte, *this;
	}
	Register8& operator--()
	{
		return --Byte, *this;
	}
	uint8_t operator*()
	{
		return Byte;
	}
	uint8_t operator*() const
	{
		return Byte;
	}
	bool operator==(const Register8& Other)
	{
		return Byte == Other.Byte;
	}
	void operator=(const Register8& Other)
	{
		Byte = Other.Byte;
	}
	void operator+=(const Register8& Other)
	{
		Byte += Other.Byte;
	}
	void operator+=(uint8_t Value)
	{
		Byte += Value;
	}
	void operator-=(const Register8& Other)
	{
		Byte -= Other.Byte;
	}
	void operator-=(uint8_t Value)
	{
		Byte -= Value;
	}

	friend std::ostream& operator<<(std::ostream& os, const Register8& Register)
	{
		os.write(reinterpret_cast<const char*>(&Register), sizeof(Register8));
		return os;
	}
	friend std::istream& operator>>(std::istream& is, Register8& Register)
	{
		is.read(reinterpret_cast<char*>(&Register), sizeof(Register8));
		return is;
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
	uint16_t operator*() const
	{
		return Word;
	}
	bool operator==(const Register16& Other)
	{
		return Word == Other.Word;
	}

	void operator=(uint16_t Value)
	{
		Word = Value;
	}
	void operator=(const Register16& Other)
	{
		Word = Other.Word;
	}
	void operator+=(const Register16& Other)
	{
		Word += Other.Word;
	}
	void operator+=(uint16_t Value)
	{
		Word += Value;
	}
	void operator-=(const Register16& Other)
	{
		Word -= Other.Word;
	}
	void operator-=(uint16_t Value)
	{
		Word -= Value;
	}

	friend std::ostream& operator<<(std::ostream& os, const Register16& Register)
	{
		os << Register.L << Register.H;
		return os;
	}
	friend std::istream& operator>>(std::istream& is, Register16& Register)
	{
		is >> Register.L >> Register.H;
		return is;
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
		return H.Byte;
	}
	uint8_t GetR() const
	{
		return L.Byte;
	}
	void IncrementR()
	{
		L.Byte = ((GetR() & 0x80) | ((GetR() + 1) & 0x7F));
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

namespace EZ80_Flag
{
	enum Type : uint8_t
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
}

#define Z80_CF		(1 << EZ80_Flag::Carry)
#define Z80_NF		(1 << EZ80_Flag::Add_Subtract)
#define Z80_VF		(1 << EZ80_Flag::Parity_Overflow)
#define Z80_PF		(1 << EZ80_Flag::Parity_Overflow)
#define Z80_XF		(1 << EZ80_Flag::X)
#define Z80_HF		(1 << EZ80_Flag::Half_Carry)
#define Z80_YF		(1 << EZ80_Flag::Y)
#define Z80_ZF		(1 << EZ80_Flag::Zero)
#define Z80_SF		(1 << EZ80_Flag::Sign)

class RegisterAF : public Register16
{
public:
	void operator=(uint16_t Value)
	{
		Word = Value;
	}	
	void operator=(const RegisterAF& Other)
	{
		Word = Other.Word;
	}
};

class RegisterF : public Register8
{
public:
	void operator=(uint8_t Value)
	{
		Byte = Value;
	}	
	void operator=(const Register8& Other)
	{
		Byte = Other.Byte;
	}
	void operator=(const RegisterF& Other)
	{
		Byte = Other.Byte;
	}
};
