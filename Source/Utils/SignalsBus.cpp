#include "SignalsBus.h"

FSignalsBus::FSignalsBus()
{
	memset(&Signals, ESignalState::HiZ, ESignalBus::MaxHardcodedIndex * sizeof(ESignalState::Type) * 2);
}


uint16_t FSignalsBus::GetDataOnAddressBus() const
{
	const uint16_t Address = (!!Signals[0][SIGNAL_A0])  << SIGNAL_A0  |
							 (!!Signals[0][SIGNAL_A1])  << SIGNAL_A1  |
							 (!!Signals[0][SIGNAL_A2])  << SIGNAL_A2  |
							 (!!Signals[0][SIGNAL_A3])  << SIGNAL_A3  |
							 (!!Signals[0][SIGNAL_A4])  << SIGNAL_A4  |
							 (!!Signals[0][SIGNAL_A5])  << SIGNAL_A5  |
							 (!!Signals[0][SIGNAL_A6])  << SIGNAL_A6  |
							 (!!Signals[0][SIGNAL_A7])  << SIGNAL_A7  |
							 (!!Signals[0][SIGNAL_A8])  << SIGNAL_A8  |
							 (!!Signals[0][SIGNAL_A9])  << SIGNAL_A9  |
							 (!!Signals[0][SIGNAL_A10]) << SIGNAL_A10 |
							 (!!Signals[0][SIGNAL_A11]) << SIGNAL_A11 |
							 (!!Signals[0][SIGNAL_A12]) << SIGNAL_A12 |
							 (!!Signals[0][SIGNAL_A13]) << SIGNAL_A13 |
							 (!!Signals[0][SIGNAL_A14]) << SIGNAL_A14 |
							 (!!Signals[0][SIGNAL_A15]) << SIGNAL_A15 ;
	return Address;
}

uint8_t FSignalsBus::GetDataOnDataBus() const
{
	const uint8_t Data = (!!Signals[0][BUS_D0]) << (SIGNAL_D0 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D1]) << (SIGNAL_D1 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D2]) << (SIGNAL_D2 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D3]) << (SIGNAL_D3 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D4]) << (SIGNAL_D4 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D5]) << (SIGNAL_D5 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D6]) << (SIGNAL_D6 - SIGNAL_D0) |
						 (!!Signals[0][BUS_D7]) << (SIGNAL_D7 - SIGNAL_D0) ;

	return Data;
}

uint8_t FSignalsBus::GetDataOnMemAddressBus() const
{
	const uint8_t Data = (!!Signals[0][BUS_MA0]) << (SIGNAL_MA0 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA1]) << (SIGNAL_MA1 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA2]) << (SIGNAL_MA2 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA3]) << (SIGNAL_MA3 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA4]) << (SIGNAL_MA4 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA5]) << (SIGNAL_MA5 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA6]) << (SIGNAL_MA6 - SIGNAL_MA0) |
						 (!!Signals[0][BUS_MA7]) << (SIGNAL_MA7 - SIGNAL_MA0) ;

	return Data;
}

uint8_t FSignalsBus::GetDataOnMemDataBus() const
{
	const uint8_t Data = (!!Signals[0][BUS_MD0]) << (SIGNAL_MD0 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD1]) << (SIGNAL_MD1 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD2]) << (SIGNAL_MD2 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD3]) << (SIGNAL_MD3 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD4]) << (SIGNAL_MD4 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD5]) << (SIGNAL_MD5 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD6]) << (SIGNAL_MD6 - SIGNAL_MD0) |
						 (!!Signals[0][BUS_MD7]) << (SIGNAL_MD7 - SIGNAL_MD0) ;

	return Data;
}

void FSignalsBus::SetDataOnAddressBus(uint16_t Address)
{
	std::memcpy(&Signals[1][BUS_A0], &Signals[0][BUS_A0], 16 * sizeof(ESignalState::Type));

	Signals[0][BUS_A0]  = !!(Address & (1 << SIGNAL_A0))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A1]  = !!(Address & (1 << SIGNAL_A1))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A2]  = !!(Address & (1 << SIGNAL_A2))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A3]  = !!(Address & (1 << SIGNAL_A3))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A4]  = !!(Address & (1 << SIGNAL_A4))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A5]  = !!(Address & (1 << SIGNAL_A5))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A6]  = !!(Address & (1 << SIGNAL_A6))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A7]  = !!(Address & (1 << SIGNAL_A7))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A8]  = !!(Address & (1 << SIGNAL_A8))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A9]  = !!(Address & (1 << SIGNAL_A9))  ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A10] = !!(Address & (1 << SIGNAL_A10)) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A11] = !!(Address & (1 << SIGNAL_A11)) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A12] = !!(Address & (1 << SIGNAL_A12)) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A13] = !!(Address & (1 << SIGNAL_A13)) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A14] = !!(Address & (1 << SIGNAL_A14)) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_A15] = !!(Address & (1 << SIGNAL_A15)) ? ESignalState::High : ESignalState::Low;
}

void FSignalsBus::SetDataOnDataBus(uint8_t Data)
{
	std::memcpy(&Signals[1][SIGNAL_D0], &Signals[0][SIGNAL_D0 ], 8 * sizeof(ESignalState::Type));

	Signals[0][BUS_D0] = !!(Data & (1 << (SIGNAL_D0 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D1] = !!(Data & (1 << (SIGNAL_D1 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D2] = !!(Data & (1 << (SIGNAL_D2 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D3] = !!(Data & (1 << (SIGNAL_D3 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D4] = !!(Data & (1 << (SIGNAL_D4 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D5] = !!(Data & (1 << (SIGNAL_D5 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D6] = !!(Data & (1 << (SIGNAL_D6 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_D7] = !!(Data & (1 << (SIGNAL_D7 - SIGNAL_D0))) ? ESignalState::High : ESignalState::Low;
}

void FSignalsBus::SetDataOnMemAddressBus(uint8_t Address)
{
	std::memcpy(&Signals[1][SIGNAL_MA0], &Signals[0][SIGNAL_MA0], 8 * sizeof(ESignalState::Type));

	Signals[0][BUS_MA0] = !!(Address & (1 << (SIGNAL_MA0 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA1] = !!(Address & (1 << (SIGNAL_MA1 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA2] = !!(Address & (1 << (SIGNAL_MA2 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA3] = !!(Address & (1 << (SIGNAL_MA3 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA4] = !!(Address & (1 << (SIGNAL_MA4 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA5] = !!(Address & (1 << (SIGNAL_MA5 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA6] = !!(Address & (1 << (SIGNAL_MA6 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MA7] = !!(Address & (1 << (SIGNAL_MA7 - SIGNAL_MA0))) ? ESignalState::High : ESignalState::Low;
}

void FSignalsBus::SetDataOnMemDataBus(uint8_t Address)
{
	std::memcpy(&Signals[1][SIGNAL_MD0], &Signals[0][SIGNAL_MD0], 8 * sizeof(ESignalState::Type));

	Signals[0][BUS_MD0] = !!(Address & (1 << (SIGNAL_MD0 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD1] = !!(Address & (1 << (SIGNAL_MD1 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD2] = !!(Address & (1 << (SIGNAL_MD2 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD3] = !!(Address & (1 << (SIGNAL_MD3 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD4] = !!(Address & (1 << (SIGNAL_MD4 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD5] = !!(Address & (1 << (SIGNAL_MD5 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD6] = !!(Address & (1 << (SIGNAL_MD6 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
	Signals[0][BUS_MD7] = !!(Address & (1 << (SIGNAL_MD7 - SIGNAL_MD0))) ? ESignalState::High : ESignalState::Low;
}

void FSignalsBus::SetAllControlOutput(ESignalState::Type State)
{
	Signals[1][BUS_M1]		= Signals[0][BUS_M1];
	Signals[1][BUS_MREQ]	= Signals[0][BUS_MREQ];
	Signals[1][BUS_IORQ]	= Signals[0][BUS_IORQ];
	Signals[1][BUS_RD]		= Signals[0][BUS_RD];
	Signals[1][BUS_WR]		= Signals[0][BUS_WR];
	Signals[1][BUS_RFSH]	= Signals[0][BUS_RFSH];
	Signals[1][BUS_HALT]	= Signals[0][BUS_HALT];
	Signals[1][BUS_BUSACK]	= Signals[0][BUS_BUSACK];

	Signals[0][BUS_M1]		= State;
	Signals[0][BUS_MREQ]	= State;
	Signals[0][BUS_IORQ]	= State;
	Signals[0][BUS_RD]		= State;
	Signals[0][BUS_WR]		= State;
	Signals[0][BUS_RFSH]	= State;
	Signals[0][BUS_HALT]	= State;
	Signals[0][BUS_BUSACK]	= State;
}

ESignalState::Type operator||(ESignalState::Type Lhs, ESignalState::Type Rhs)
{
	if (Lhs == ESignalState::Low && Rhs == ESignalState::Low)
	{
		return ESignalState::Low;
	}
	else if (Lhs == ESignalState::HiZ && Rhs == ESignalState::HiZ)
	{
		return ESignalState::HiZ;
	}
	return ESignalState::High;
}

ESignalState::Type operator&&(ESignalState::Type Lhs, ESignalState::Type Rhs)
{
	if (Lhs == ESignalState::High && Rhs == ESignalState::High)
	{
		return ESignalState::High;
	}
	else if (Lhs == ESignalState::HiZ && Rhs == ESignalState::HiZ)
	{
		return ESignalState::HiZ;
	}
	return ESignalState::Low;
}

ESignalState::Type operator!(ESignalState::Type Value)
{
	switch (Value)
	{
	case ESignalState::Low:		return ESignalState::High;
	case ESignalState::High:	return ESignalState::Low;
	}
	return ESignalState::HiZ;
}
