#pragma once

#include <CoreMinimal.h>

// Define a message as an enumeration.
#define REGISTER_BUS_NAME(num,name) name = num,
namespace ESignalBus
{
	enum Type : int32_t
	{
		// Include all the hard-coded names
		#include "SignalsBusNames.inl"

		// Special constant for the last hard-coded name index
		MaxHardcodedIndex,
	};
}
#undef REGISTER_BUS_NAME

#define REGISTER_BUS_NAME(num,name) inline constexpr ESignalBus::Type BUS_##name = ESignalBus::name;
#include "SignalsBusNames.inl"
#undef REGISTER_BUS_NAME

namespace ESignalState
{
	enum Type : int32_t
	{
		Low		= 0,
		High	= 1,
		HiZ		= -1,
	};
}

ESignalState::Type operator||(ESignalState::Type Lhs, ESignalState::Type Rhs);

ESignalState::Type operator&&(ESignalState::Type Lhs, ESignalState::Type Rhs);

class FSignalsBus
{
public:
	FSignalsBus();

	FORCEINLINE bool IsActive(ESignalBus::Type Signal, ESignalState::Type ActiveSignal = ESignalState::Low) const
	{
		return Signals[0][Signal] == ActiveSignal;
	}
	FORCEINLINE bool IsInactive(ESignalBus::Type Signal, ESignalState::Type ActiveSignal = ESignalState::Low) const
	{
		return Signals[0][Signal] != ActiveSignal;
	}

	FORCEINLINE void SetSignal(ESignalBus::Type Signal, ESignalState::Type State)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = State;
	}
	FORCEINLINE void SetActive(ESignalBus::Type Signal, ESignalState::Type ActiveSignal = ESignalState::Low)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = ActiveSignal;
	}
	FORCEINLINE void SetInactive(ESignalBus::Type Signal, ESignalState::Type InactiveSignal = ESignalState::High)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = InactiveSignal;
	}
	FORCEINLINE void SetHighImpedance(ESignalBus::Type Signal)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = ESignalState::HiZ;
	}

	FORCEINLINE bool IsPositiveEdge(ESignalBus::Type Signal) // -> low-to-high transition
	{
		return /*last*/Signals[1][Signal] == ESignalState::Low && /*current*/Signals[0][Signal] == ESignalState::High;
	}
	FORCEINLINE bool IsNegativeEdge(ESignalBus::Type Signal) // -> high-to-low transition
	{
		return /*last*/Signals[1][Signal] == ESignalState::High && /*current*/Signals[0][Signal] == ESignalState::Low;
	}

	FORCEINLINE ESignalState::Type GetSignal(ESignalBus::Type Signal) const
	{
		return Signals[0][Signal];
	}

	uint16_t GetDataOnAddressBus() const;
	uint8_t GetDataOnDataBus() const;

	void SetDataOnAddressBus(uint16_t Address);
	void SetDataOnDataBus(uint8_t Data);

	void SetAllControlOutput(ESignalState::Type State);

private:
	ESignalState::Type Signals[2][ESignalBus::MaxHardcodedIndex];
};
