#pragma once

#include <CoreMinimal.h>

// Define a message as an enumeration.
#define REGISTER_BUS_NAME(num,name) name = num,
namespace ESignalBus
{
	enum Type
	{
		// Include all the hard-coded names
		#include "SignalsBusNames.inl"

		// Special constant for the last hard-coded name index
		MaxHardcodedNameIndex,
	};
}
#undef REGISTER_BUS_NAME

#define REGISTER_BUS_NAME(num,name) inline constexpr ESignalBus::Type BUS_##name = ESignalBus::name;
#include "SignalsBusNames.inl"
#undef REGISTER_BUS_NAME

namespace ESignalState
{
	enum Type
	{
		Low		= 0,
		High	= 1,
		HiZ		= -1,
	};
}

struct FExtraSignals
{
	FExtraSignals(ESignalState::Type _Type = ESignalState::HiZ)
		: Prev(_Type)
		, Current(_Type)
	{}

	FExtraSignals& operator=(const FExtraSignals& Other)
	{
		Prev = Other.Prev;
		Current = Other.Current;
	}

	ESignalState::Type Prev;
	ESignalState::Type Current;
};

class FSignalsBus
{
public:
	FSignalsBus();

	inline bool IsActive(ESignalBus::Type Signal, ESignalState::Type ActiveSignal = ESignalState::Low) const
	{
		return Signals[0][Signal] == ActiveSignal;
	}
	inline bool IsActive(FName SignalName, ESignalState::Type ActiveSignal = ESignalState::Low) const
	{
		return ExtraSignals.contains(SignalName) ? ExtraSignals.at(SignalName).Current == ActiveSignal : false;
	}
	inline bool IsInactive(ESignalBus::Type Signal, ESignalState::Type ActiveSignal = ESignalState::Low) const
	{
		return Signals[0][Signal] != ActiveSignal;
	}
	inline bool IsInactive(FName SignalName, ESignalState::Type ActiveSignal = ESignalState::Low) const
	{
		return ExtraSignals.contains(SignalName) ? ExtraSignals.at(SignalName).Current != ActiveSignal : false;
	}
	inline void SetActive(ESignalBus::Type Signal, ESignalState::Type ActiveSignal = ESignalState::Low)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = ActiveSignal;
	}
	inline void SetActive(FName SignalName, ESignalState::Type ActiveSignal = ESignalState::Low)
	{
		if (ExtraSignals.contains(SignalName))
		{
			ExtraSignals[SignalName].Prev = ExtraSignals[SignalName].Current;
			ExtraSignals[SignalName].Current = ActiveSignal;
		}
	}
	inline void SetInactive(ESignalBus::Type Signal, ESignalState::Type InactiveSignal = ESignalState::High)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = InactiveSignal;
	}
	inline void SetInactive(FName SignalName, ESignalState::Type InactiveSignal = ESignalState::High)
	{
		if (ExtraSignals.contains(SignalName))
		{
			ExtraSignals[SignalName].Prev = ExtraSignals[SignalName].Current;
			ExtraSignals[SignalName].Current = InactiveSignal;
		}
	}
	inline void SetHighImpedance(ESignalBus::Type Signal)
	{
		Signals[1][Signal] = Signals[0][Signal];
		Signals[0][Signal] = ESignalState::HiZ;
	}
	inline void SetHighImpedance(FName SignalName)
	{
		if (ExtraSignals.contains(SignalName))
		{
			ExtraSignals[SignalName].Prev = ExtraSignals[SignalName].Current;
			ExtraSignals[SignalName].Current = ESignalState::HiZ;
		}
	}

	inline bool IsPositiveEdge(ESignalBus::Type Signal) // -> low-to-high transition
	{
		return /*last*/Signals[1][Signal] == ESignalState::Low && /*current*/Signals[0][Signal] == ESignalState::High;
	}
	inline bool IsPositiveEdge(FName SignalName) // -> low-to-high transition
	{
		return ExtraSignals.contains(SignalName) ? /*last*/   ExtraSignals.at(SignalName).Prev    == ESignalState::Low	&&
												   /*current*/ExtraSignals.at(SignalName).Current == ESignalState::High
												 : false;
	}
	inline bool IsNegativeEdge(ESignalBus::Type Signal) // -> high-to-low transition
	{
		return /*last*/Signals[1][Signal] == ESignalState::High && /*current*/Signals[0][Signal] == ESignalState::Low;
	}
	inline bool IsNegativeEdge(FName SignalName) // -> high-to-low transition
	{
		return ExtraSignals.contains(SignalName) ? /*last*/   ExtraSignals.at(SignalName).Prev    == ESignalState::High &&
												   /*current*/ExtraSignals.at(SignalName).Current == ESignalState::Low
												 : false;
	}
	
	inline ESignalState::Type GetSignal(ESignalBus::Type Signal) const
	{
		return Signals[0][Signal];
	}
	inline ESignalState::Type GetSignal(FName SignalName) const
	{
		return ExtraSignals.contains(SignalName) ? ExtraSignals.at(SignalName).Current : ESignalState::HiZ;
	}

	void SetSignal(ESignalBus::Type Signal, ESignalState::Type State);
	void SetSignal(FName SignalName, ESignalState::Type State);

	uint16_t GetDataOnAddressBus() const;
	uint8_t GetDataOnDataBus() const;

	void SetDataOnAddressBus(uint16_t Address);
	void SetDataOnDataBus(uint8_t Data);

	void SetAllControlOutput(ESignalState::Type State);

	void AddSignal(FName SignalName);

private:
	ESignalState::Type Signals[2][ESignalBus::MaxHardcodedNameIndex];
	std::map<FName, FExtraSignals> ExtraSignals;
};
