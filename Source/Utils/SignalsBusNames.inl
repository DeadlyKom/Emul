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

// external address bus signals, not the CPU address bus
#define SIGNAL_A16		(37)
#define SIGNAL_A17		(38)
#define SIGNAL_A18		(39)
#define SIGNAL_A19		(40)
#define SIGNAL_A20		(41)
#define SIGNAL_A21		(42)
#define SIGNAL_A22		(43)
#define SIGNAL_A23		(44)

// address signals
REGISTER_BUS_NAME(SIGNAL_A0, A0)
REGISTER_BUS_NAME(SIGNAL_A1, A1)
REGISTER_BUS_NAME(SIGNAL_A2, A2)
REGISTER_BUS_NAME(SIGNAL_A3, A3)
REGISTER_BUS_NAME(SIGNAL_A4, A4)
REGISTER_BUS_NAME(SIGNAL_A5, A5)
REGISTER_BUS_NAME(SIGNAL_A6, A6)
REGISTER_BUS_NAME(SIGNAL_A7, A7)
REGISTER_BUS_NAME(SIGNAL_A8, A8)
REGISTER_BUS_NAME(SIGNAL_A9, A9)
REGISTER_BUS_NAME(SIGNAL_A10, A10)
REGISTER_BUS_NAME(SIGNAL_A11, A11)
REGISTER_BUS_NAME(SIGNAL_A12, A12)
REGISTER_BUS_NAME(SIGNAL_A13, A13)
REGISTER_BUS_NAME(SIGNAL_A14, A14)
REGISTER_BUS_NAME(SIGNAL_A15, A15)

// external address bus signals, not the CPU address bus
REGISTER_BUS_NAME(SIGNAL_A16, A16)
REGISTER_BUS_NAME(SIGNAL_A17, A17)
REGISTER_BUS_NAME(SIGNAL_A18, A18)
REGISTER_BUS_NAME(SIGNAL_A19, A19)
REGISTER_BUS_NAME(SIGNAL_A20, A20)
REGISTER_BUS_NAME(SIGNAL_A21, A21)
REGISTER_BUS_NAME(SIGNAL_A22, A22)
REGISTER_BUS_NAME(SIGNAL_A23, A23)

// data signals
REGISTER_BUS_NAME(SIGNAL_D0, D0)
REGISTER_BUS_NAME(SIGNAL_D1, D1)
REGISTER_BUS_NAME(SIGNAL_D2, D2)
REGISTER_BUS_NAME(SIGNAL_D3, D3)
REGISTER_BUS_NAME(SIGNAL_D4, D4)
REGISTER_BUS_NAME(SIGNAL_D5, D5)
REGISTER_BUS_NAME(SIGNAL_D6, D6)
REGISTER_BUS_NAME(SIGNAL_D7, D7)

// system control signals
REGISTER_BUS_NAME(SIGNAL_M1, M1)
REGISTER_BUS_NAME(SIGNAL_MREQ, MREQ)
REGISTER_BUS_NAME(SIGNAL_IORQ, IORQ)
REGISTER_BUS_NAME(SIGNAL_RD, RD)
REGISTER_BUS_NAME(SIGNAL_WR, WR)
REGISTER_BUS_NAME(SIGNAL_RFSH, RFSH)

// CPU control signals
REGISTER_BUS_NAME(SIGNAL_HALT, HALT)
REGISTER_BUS_NAME(SIGNAL_WAIT, WAIT)
REGISTER_BUS_NAME(SIGNAL_INT, INT)
REGISTER_BUS_NAME(SIGNAL_NMI, NMI)
REGISTER_BUS_NAME(SIGNAL_RESET, RESET)

// CPU bus control signals
REGISTER_BUS_NAME(SIGNAL_BUSRQ, BUSRQ)
REGISTER_BUS_NAME(SIGNAL_BUSACK, BUSACK)
