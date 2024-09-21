#pragma once

// address pins
#define Z80_PIN_A0		(0)
#define Z80_PIN_A1		(1)
#define Z80_PIN_A2		(2)
#define Z80_PIN_A3		(3)
#define Z80_PIN_A4		(4)
#define Z80_PIN_A5		(5)
#define Z80_PIN_A6		(6)
#define Z80_PIN_A7		(7)
#define Z80_PIN_A8		(8)
#define Z80_PIN_A9		(9)
#define Z80_PIN_A10		(10)
#define Z80_PIN_A11		(11)
#define Z80_PIN_A12		(12)
#define Z80_PIN_A13		(13)
#define Z80_PIN_A14		(14)
#define Z80_PIN_A15		(15)

// data pins
#define Z80_PIN_D0		(16)
#define Z80_PIN_D1		(17)
#define Z80_PIN_D2		(18)
#define Z80_PIN_D3		(19)
#define Z80_PIN_D4		(20)
#define Z80_PIN_D5		(21)
#define Z80_PIN_D6		(22)
#define Z80_PIN_D7		(23)

// system control pins
#define Z80_PIN_M1		(24)	// machine cycle 1
#define Z80_PIN_MREQ	(25)	// memory request
#define Z80_PIN_IORQ	(26)	// input/output request
#define Z80_PIN_RD		(27)	// read
#define Z80_PIN_WR		(28)	// write
#define Z80_PIN_RFSH	(29)	// refresh

// CPU control pins
#define Z80_PIN_HALT	(30)	// halt state
#define Z80_PIN_WAIT	(31)	// wait requested
#define Z80_PIN_INT		(32)	// interrupt request
#define Z80_PIN_NMI		(33)	// non-maskable interrupt
#define Z80_PIN_RESET	(34)	// reset requested

// CPU bus control pins
#define Z80_PIN_BUSRQ	(35)	// 
#define Z80_PIN_BUSACK	(36)	// 

// dummy
#define Z80_PIN_DUMMY0	(37)	// reserve
#define Z80_PIN_DUMMY1	(38)	// reserve
#define Z80_PIN_DUMMY2	(39)	// reserve

// pin bit masks
#define Z80_A0			(1ULL<<Z80_PIN_A0)
#define Z80_A1			(1ULL<<Z80_PIN_A1)
#define Z80_A2			(1ULL<<Z80_PIN_A2)
#define Z80_A3			(1ULL<<Z80_PIN_A3)
#define Z80_A4			(1ULL<<Z80_PIN_A4)
#define Z80_A5			(1ULL<<Z80_PIN_A5)
#define Z80_A6			(1ULL<<Z80_PIN_A6)
#define Z80_A7			(1ULL<<Z80_PIN_A7)
#define Z80_A8			(1ULL<<Z80_PIN_A8)
#define Z80_A9			(1ULL<<Z80_PIN_A9)
#define Z80_A10			(1ULL<<Z80_PIN_A10)
#define Z80_A11			(1ULL<<Z80_PIN_A11)
#define Z80_A12			(1ULL<<Z80_PIN_A12)
#define Z80_A13			(1ULL<<Z80_PIN_A13)
#define Z80_A14			(1ULL<<Z80_PIN_A14)
#define Z80_A15			(1ULL<<Z80_PIN_A15)
#define Z80_D0			(1ULL<<Z80_PIN_D0)
#define Z80_D1			(1ULL<<Z80_PIN_D1)
#define Z80_D2			(1ULL<<Z80_PIN_D2)
#define Z80_D3			(1ULL<<Z80_PIN_D3)
#define Z80_D4			(1ULL<<Z80_PIN_D4)
#define Z80_D5			(1ULL<<Z80_PIN_D5)
#define Z80_D6			(1ULL<<Z80_PIN_D6)
#define Z80_D7			(1ULL<<Z80_PIN_D7)
#define Z80_M1			(1ULL<<Z80_PIN_M1)
#define Z80_MREQ		(1ULL<<Z80_PIN_MREQ)
#define Z80_IORQ		(1ULL<<Z80_PIN_IORQ)
#define Z80_RD			(1ULL<<Z80_PIN_RD)
#define Z80_WR			(1ULL<<Z80_PIN_WR)
#define Z80_RFSH		(1ULL<<Z80_PIN_RFSH)
#define Z80_HALT		(1ULL<<Z80_PIN_HALT)
#define Z80_WAIT		(1ULL<<Z80_PIN_WAIT)
#define Z80_INT			(1ULL<<Z80_PIN_INT)
#define Z80_NMI			(1ULL<<Z80_PIN_NMI)
#define Z80_RESET		(1ULL<<Z80_PIN_RESET)
#define Z80_BUSRQ		(1ULL<<Z80_PIN_BUSRQ)
#define Z80_BUSACK		(1ULL<<Z80_PIN_BUSACK)

#define Z80_SYS_CTRL_PIN_MASK (Z80_M1|Z80_MREQ|Z80_IORQ|Z80_RD|Z80_WR|Z80_RFSH)
#define Z80_CPU_CTRL_PIN_MASK (Z80_HALT|Z80_WAIT|Z80_INT|Z80_NMI|Z80_RESET)
#define Z80_BUS_CTRL_PIN_MASK (Z80_BUSRQ|Z80_BUSACK)
#define Z80_PIN_MASK ((1ULL<<40)-1)

// pin access helper macros
#define Z80_MAKE_PINS(ctrl, addr, data) ((ctrl)|((data&0xFF)<<16)|((addr)&0xFFFFULL))
#define Z80_GET_ADDR(p)					((uint16_t)(p))
#define Z80_SET_ADDR(p,a)				{p=((p)&~0xFFFF)|((a)&0xFFFF);}
#define Z80_GET_DATA(p)					((uint8_t)((p)>>16))
#define Z80_SET_DATA(p,d)				{p=((p)&~0xFF0000ULL)|(((d)<<16)&0xFF0000ULL);}
