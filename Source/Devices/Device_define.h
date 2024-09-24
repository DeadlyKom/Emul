#pragma once

// address signals
#define SIGNAL_A0		(0)
#define SIGNAL_A1		(1)
#define SIGNAL_A2		(2)
#define SIGNAL_A3		(3)
#define SIGNAL_A4		(4)
#define SIGNAL_A5		(5)
#define SIGNAL_A6		(6)
#define SIGNAL_A7		(7)
#define SIGNAL_A8		(8)
#define SIGNAL_A9		(9)
#define SIGNAL_A10		(10)
#define SIGNAL_A11		(11)
#define SIGNAL_A12		(12)
#define SIGNAL_A13		(13)
#define SIGNAL_A14		(14)
#define SIGNAL_A15		(15)

// data signals
#define SIGNAL_D0		(16)
#define SIGNAL_D1		(17)
#define SIGNAL_D2		(18)
#define SIGNAL_D3		(19)
#define SIGNAL_D4		(20)
#define SIGNAL_D5		(21)
#define SIGNAL_D6		(22)
#define SIGNAL_D7		(23)

// system control signals
#define SIGNAL_M1		(24)	// machine cycle 1
#define SIGNAL_MREQ		(25)	// memory request
#define SIGNAL_IORQ		(26)	// input/output request
#define SIGNAL_RD		(27)	// read
#define SIGNAL_WR		(28)	// write
#define SIGNAL_RFSH		(29)	// refresh

// CPU control signals
#define SIGNAL_HALT		(30)	// halt state
#define SIGNAL_WAIT		(31)	// wait requested
#define SIGNAL_INT		(32)	// interrupt request
#define SIGNAL_NMI		(33)	// non-maskable interrupt
#define SIGNAL_RESET	(34)	// reset requested

// CPU bus control signals
#define SIGNAL_BUSRQ	(35)	// 
#define SIGNAL_BUSACK	(36)	// 

// dummy
#define SIGNAL_DUMMY0	(37)	// reserve
#define SIGNAL_DUMMY1	(38)	// reserve
#define SIGNAL_DUMMY2	(39)	// reserve

// bus bit masks
#define BUS_A0			(1ULL << SIGNAL_A0)
#define BUS_A1			(1ULL << SIGNAL_A1)
#define BUS_A2			(1ULL << SIGNAL_A2)
#define BUS_A3			(1ULL << SIGNAL_A3)
#define BUS_A4			(1ULL << SIGNAL_A4)
#define BUS_A5			(1ULL << SIGNAL_A5)
#define BUS_A6			(1ULL << SIGNAL_A6)
#define BUS_A7			(1ULL << SIGNAL_A7)
#define BUS_A8			(1ULL << SIGNAL_A8)
#define BUS_A9			(1ULL << SIGNAL_A9)
#define BUS_A10			(1ULL << SIGNAL_A10)
#define BUS_A11			(1ULL << SIGNAL_A11)
#define BUS_A12			(1ULL << SIGNAL_A12)
#define BUS_A13			(1ULL << SIGNAL_A13)
#define BUS_A14			(1ULL << SIGNAL_A14)
#define BUS_A15			(1ULL << SIGNAL_A15)
#define BUS_D0			(1ULL << SIGNAL_D0)
#define BUS_D1			(1ULL << SIGNAL_D1)
#define BUS_D2			(1ULL << SIGNAL_D2)
#define BUS_D3			(1ULL << SIGNAL_D3)
#define BUS_D4			(1ULL << SIGNAL_D4)
#define BUS_D5			(1ULL << SIGNAL_D5)
#define BUS_D6			(1ULL << SIGNAL_D6)
#define BUS_D7			(1ULL << SIGNAL_D7)
#define BUS_M1			(1ULL << SIGNAL_M1)
#define BUS_MREQ		(1ULL << SIGNAL_MREQ)
#define BUS_IORQ		(1ULL << SIGNAL_IORQ)
#define BUS_RD			(1ULL << SIGNAL_RD)
#define BUS_WR			(1ULL << SIGNAL_WR)
#define BUS_RFSH		(1ULL << SIGNAL_RFSH)
#define BUS_HALT		(1ULL << SIGNAL_HALT)
#define BUS_WAIT		(1ULL << SIGNAL_WAIT)
#define BUS_INT			(1ULL << SIGNAL_INT)
#define BUS_NMI			(1ULL << SIGNAL_NMI)
#define BUS_RESET		(1ULL << SIGNAL_RESET)
#define BUS_BUSRQ		(1ULL << SIGNAL_BUSRQ)
#define BUS_BUSACK		(1ULL << SIGNAL_BUSACK)

#define T1				0
#define T2				1
#define T3				2
#define T4				3
#define T5				4
#define T6				5
#define CTC				uint8_t(-1)	// completed tick cycle

// tict half-clock
#define T1_H1			0
#define T1_H2			1
#define T2_H1			2
#define T2_H2			3
#define T3_H1			4
#define T3_H2			5
#define T4_H1			6
#define T4_H2			7
#define T5_H1			8
#define T5_H2			9
#define T6_H1			10
#define T6_H2			11

#define M1				0
#define M2				1
#define M3				2
#define M4				3
#define M5				4
#define M6				5

#define SYS_CTRL_MASK											(BUS_M1	   | BUS_MREQ   | BUS_IORQ | BUS_RD  | BUS_WR    | BUS_RFSH)
#define CPU_CTRL_MASK											(BUS_HALT  | BUS_WAIT   | BUS_INT  | BUS_NMI | BUS_RESET)
#define BUS_CTRL_MASK											(BUS_BUSRQ | BUS_BUSACK)
#define CTRL_OUTPUT_MASK										(BUS_M1	   | BUS_MREQ   | BUS_IORQ | BUS_RD  | BUS_WR    | BUS_RFSH | BUS_HALT | BUS_BUSACK)
#define CTRL_INPUT_MASK											(BUS_WAIT  | BUS_INT    | BUS_NMI  | BUS_RESET | BUS_BUSRQ)
#define ALL_CTRL_MASK											(~(MAKE_SYS_CTRL_MASK | MAKE_CPU_CTRL_MASK | MAKE_BUS_CTRL_MASK))
#define BUS_MASK												((1ULL << 40) - 1)

// signals access helper macros
#define BUS_SET_SIGNAL(signals, signal)								(signals |= (signal))
#define BUS_RESET_SIGNAL(signals, signal)							(signals &= ~(signal))
#define BUS_GET_SIGNAL(signals, signal)								(signals & (signal))
#define BUS_SET_ACTIVE_SIGNAL(signals, signal)						(BUS_RESET_SIGNAL(signals, signal))
#define BUS_SET_INACTIVE_SIGNAL(signals, signal)					(BUS_SET_SIGNAL(signals, signal))

#define BUS_MAKE_SIGNAL(ctrl, adr, data)							((ctrl)|((data&0xFF)<<16)|((adr)&0xFFFFULL))

#define BUS_GET_ADR(signals)										((uint16_t)(signals))
#define BUS_SET_ADR(signals,adr)									{signals=((signals)&~0xFFFF)|((adr)&0xFFFF);}
#define BUS_GET_DATA(signals)										((uint8_t)((signals)>>16))
#define BUS_SET_DATA(signals,data)									{signals=((signals)&~0xFF0000ULL)|(((data)<<16)&0xFF0000ULL);}

#define BUS_IS_ACTIVE_RESET(signals)								(!(signals & BUS_RESET))
#define BUS_IS_ACTIVE_WAIT(signals)									(!(signals & BUS_WAIT))
#define BUS_IS_INACTIVE_WAIT(signals)								(signals & BUS_WAIT)
