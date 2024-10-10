#include "Z80_NMOS.h"
#include "Utils/SignalsBus.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))

namespace
{
    static const char* ThisDeviceName = "CPU Z80 NMOS";
}

FCPU_Z80_NMOS::FCPU_Z80_NMOS()
    : FDevice(DEVICE_NAME(), EName::Z80, EDeviceType::CPU)
{
	memset(&Registers, 0, sizeof(FInternalRegisters));
}

void FCPU_Z80_NMOS::Tick()
{
    Clock();
    Clock();

    Registers.CC++;
}

void FCPU_Z80_NMOS::Reset()
{}

FRegisters FCPU_Z80_NMOS::GetRegisters() const
{
    constexpr int32_t AF = 0;
    constexpr int32_t AF_ = 1;
    constexpr int32_t BC_ = 2;
    constexpr int32_t BC = 3;
    constexpr int32_t HL = 4;
    constexpr int32_t HL_ = 5;
    constexpr int32_t DE = 6;
    constexpr int32_t DE_ = 7;
    constexpr int32_t IX = 8;
    constexpr int32_t IY = 9;
    constexpr int32_t SP = 10;
    constexpr int32_t WZ = 11;

    constexpr int32_t PC = 0;
    constexpr int32_t IR = 1;

    FRegisters RegistersState
    {
        .PC = Registers.regs2_[PC][0],
        .IR = Registers.regs2_[IR][0],

        .IX = Registers.regs_[IX][0],
        .IY = Registers.regs_[IY][0],
        .SP = Registers.regs_[SP][0],

        .AF =  Registers.w361 ? Registers.regs_[AF_][0] : Registers.regs_[AF][0],
        .HL = !Registers.w327 ? Registers.regs_[HL_][0] : Registers.regs_[HL][0],
        .DE = !Registers.w327 ? Registers.regs_[DE_][0] : Registers.regs_[DE][0],
        .BC = !Registers.w327 ? Registers.regs_[BC_][0] : Registers.regs_[BC][0],

        .AF_ =  Registers.w361 ? Registers.regs_[AF][0] : Registers.regs_[AF_][0],
        .HL_ = !Registers.w327 ? Registers.regs_[HL][0] : Registers.regs_[HL_][0],
        .DE_ = !Registers.w327 ? Registers.regs_[DE][0] : Registers.regs_[DE_][0],
        .BC_ = !Registers.w327 ? Registers.regs_[BC][0] : Registers.regs_[BC_][0],
    };
    return RegistersState;
}

void FCPU_Z80_NMOS::ClkLatches(bool bClk)
{
    Registers.w304 = !bClk &&  Registers.w303 && Registers.pla[95];
    Registers.w328 = !bClk && !Registers.w302;
    Registers.w440 = !bClk && !Registers.w392;
    Registers.w329 = !bClk && !Registers.w326 && !Registers.w327;
    Registers.w331 = !bClk && !Registers.w326 &&  Registers.w327;
}

void FCPU_Z80_NMOS::ResetLogic(bool bClk)
{
    Registers.w52 = !bClk && Registers.l19;
    if (!bClk)
    {
        Registers.w50 = SB->IsActive(BUS_RESET);
    }
    else
    {
        Registers.w51 = Registers.w50;
    }
    if (Registers.w52)
    {
        Registers.w54 = !Registers.w51;
    }

    Registers.w55 = !Registers.w54;
    Registers.w53 = !bClk && !Registers.l20 && !Registers.w55;

    if (Registers.w53 && Registers.w104 && !Registers.w51)
    {
        Registers.w56 = true;
    }
    if (Registers.w53 && Registers.w51)
    {
        Registers.w56 = false;
    }
    if (Registers.w55)
    {
        Registers.w56 = true;
    }
}

void FCPU_Z80_NMOS::StateLogic(bool bClk)
{
    Registers.w132 = !bClk && Registers.l36;
    if (bClk)
    {
        Registers.w59 = Registers.w58;
    }
    if (!bClk)
    {
        Registers.w63 = Registers.l23;
    }

    Registers.w65 = !(!Registers.l24 || bClk || !Registers.w59);
    Registers.w67 = !bClk && !Registers.w59;

    if (Registers.w63 || Registers.w67)
    {
        Registers.w66 = false;
    }
    if (Registers.w65)
    {
        Registers.w66 = true;
    }
    if (bClk)
    {
        Registers.l12 = !Registers.w41 && !(!Registers.w114 && Registers.w34);
    }
    else
    {
        Registers.w34 = !(Registers.l12 && Registers.w112);
    }
    if (bClk)
    {
        Registers.l30 = Registers.w41;
    }

    Registers.w530 = !(Registers.l30 && Registers.w112);

    if (!bClk)
    {
        Registers.w109 = !Registers.w530;
    }

    Registers.w113 = Registers.w66 || Registers.w63 || Registers.w65;

    if (!bClk)
    {
        Registers.w111 = Registers.w112;
    }

    Registers.w110 = !(Registers.w113 || Registers.w111);

    if (bClk)
    {
        Registers.l31 = Registers.w110;
    }
    if (!bClk)
    {
        Registers.w114 = Registers.w112 && Registers.l31;
    }
    if (!bClk)
    {
        Registers.w40 = !Registers.w39;
    }
    if (Registers.w202)
    {
        Registers.w40 = false;
    }

    Registers.w41 = !Registers.w40 && !Registers.w34;

    if (bClk)
    {
        Registers.l25 = Registers.w109;
    }
    else
    {
        Registers.w68 = Registers.l25 && Registers.w112;
    }
    if (bClk)
    {
        Registers.l21 = Registers.w68;
    }

    Registers.w60 = Registers.l21 && Registers.w112;

    if (!bClk)
    {
        Registers.w61 = Registers.w60;
    }
    if (Registers.w132)
    {
        Registers.w131 = Registers.w130;
    }
    if (bClk)
    {
        Registers.l32 = Registers.w131;
    }
    if (Registers.w132)
    {
        Registers.w120 = Registers.l32 && !Registers.w134 && !Registers.w130;
    }
    if (bClk)
    {
        Registers.l35 = Registers.w120;
    }

    Registers.w128 = !(Registers.w134 || Registers.w130 || !Registers.l35);

    if (Registers.w132)
    {
        Registers.w127 = Registers.w128;
    }
    if (bClk)
    {
        Registers.l34 = Registers.w127;
    }
    if (Registers.w132)
    {
        Registers.w123 = !Registers.w130 && (Registers.l34 || Registers.w134);
    }
    if (bClk)
    {
        Registers.l33 = Registers.w123;
    }
    
    Registers.w122 = Registers.l33 && !Registers.w130;

    if (Registers.w132)
    {
        Registers.w121 = Registers.w122;
    }

    Registers.w116 = Registers.w114 && Registers.w120 && Registers.w126;

    if (bClk)
    {
        if (Registers.w116)
        {
            Registers.w115 = true;
        }
        if (Registers.w131 || Registers.w123)
        {
            Registers.w115 = false;
        }
    }

    // reset
    if (bClk)
    {
        Registers.l19 = !(Registers.w131 && Registers.w114);
    }
    if (bClk)
    {
        Registers.l20 = !(Registers.w131 && Registers.w114);
    }
}

void FCPU_Z80_NMOS::InterruptLogic(bool bClk)
{
    Registers.w16 = Registers.l4 && !bClk;

    // nmi pin
    if (bClk)
    {
        Registers.l2 = !(Registers.w55 || Registers.w19);
    }
    if (SB->IsActive(BUS_NMI))
    {
        Registers.w7 = true;
    }
    if (!Registers.l2)
    {
        Registers.w7 = false;
    }

    if (!SB->IsActive(BUS_NMI))
    {
        Registers.w6 = Registers.w7;
    }
    if (!Registers.l2)
    {
        Registers.w6 = 0;
    }

    if (!bClk)
    {
        Registers.w8 = Registers.w6;
    }
    else
    {
        Registers.w9 = Registers.w8;
    }

    if (Registers.w16)
    {
        Registers.w19 = Registers.w9;
    }
    if (Registers.w55)
    {
        Registers.w19 = 0;
    }

    // int pin
    if (!bClk)
    {
        Registers.w4 = SB->IsActive(BUS_INT);
    }
    else
    {
        Registers.w5 = Registers.w4;
    }
    Registers.w12 = !(Registers.w5 || Registers.w9 || Registers.l3);

    if (Registers.w16)
    {
        Registers.w18 = Registers.w12;
    }
    if (Registers.w55)
    {
        Registers.w18 = false;
    }
    if (bClk)
    {
        Registers.l82 = Registers.w114;
    }

    Registers.w531 = !(Registers.w131 && Registers.w18 && Registers.l82);

    if (!bClk)
    {
        Registers.w37 = Registers.w531;
    }

    Registers.w35 = !(!Registers.w37 && Registers.w131 && Registers.w18);
}

void FCPU_Z80_NMOS::IOLogic(bool bClk)
{
    if (!bClk)
    {
        Registers.w58 = SB->IsActive(BUS_BUSRQ);
    }

    if (bClk)
    {
        Registers.l23 = Registers.w55;
    }

    SB->SetSignal(BUS_BUSACK, !(!Registers.w65 && Registers.w66 && !Registers.w67) ? ESignalState::High : ESignalState::Low);

    if (bClk)
    {
        Registers.l22 = SB->IsActive(BUS_BUSACK);
    }

    Registers.w62 = Registers.l22 || SB->IsActive(BUS_BUSACK);
    Registers.w94 = (Registers.w41 && !Registers.w131) || Registers.w55 || Registers.w109;

    if (bClk)
    {
        Registers.l9 = Registers.w94;
    }

    Registers.w32 = !bClk && Registers.l9;
    Registers.w26 = Registers.w131 && Registers.w41 && bClk;

    if (Registers.w32 || Registers.w26)
    {
        Registers.w21 = false;
    }
    if (Registers.w24)
    {
        Registers.w21 = true;
    }

    SB->SetSignal(BUS_MREQ, ESignalState::HiZ);

    if (!Registers.w62 && !Registers.w21)
    {
        SB->SetSignal(BUS_MREQ, ESignalState::High);
    }
    if (Registers.w21)
    {
        SB->SetSignal(BUS_MREQ, ESignalState::Low);
    }

    Registers.w36 = Registers.w114 && Registers.w106;

    if (bClk)
    {
        Registers.l5 = Registers.w35;
    }

    Registers.w23 = !bClk && !Registers.l5;

    if (Registers.w23 || (Registers.w36 && bClk))
    {
        Registers.w22 = 1;
    }
    if (Registers.w26 || Registers.w32)
    {
        Registers.w22 = 0;
    }

    SB->SetSignal(BUS_IORQ, ESignalState::HiZ);

    if (!Registers.w62 && !Registers.w22)
    {
        SB->SetSignal(BUS_IORQ, ESignalState::High);
    }
    if (Registers.w22)
    {
        SB->SetSignal(BUS_IORQ, ESignalState::Low);
    }

    Registers.w24 = !bClk && !Registers.w202 && !Registers.l6;
    Registers.w25 = !(Registers.w24 || Registers.w23 || (Registers.w36 && bClk));

    if (!Registers.w25 && Registers.l8)
    {
        Registers.w31 = 1;
    }
    if (Registers.w26 || Registers.w32)
    {
        Registers.w31 = 0;
    }

    SB->SetSignal(BUS_RD, ESignalState::HiZ);

    if (!Registers.w62 && !Registers.w31)
    {
        SB->SetSignal(BUS_RD, ESignalState::High);
    }
    if (Registers.w31)
    {
        SB->SetSignal(BUS_RD, ESignalState::Low);
    }

    if (bClk)
    {
        Registers.l10 = !Registers.w41 && !Registers.w55;
    }
    if (!bClk && !Registers.l10)
    {
        Registers.w33 = false;
    }
    if (bClk)
    {
        Registers.l11 = !(Registers.w114 && Registers.w201);
    }
    if (!Registers.l11 && !bClk)
    {
        Registers.w33 = true;
    }
    if (bClk && Registers.w106 && Registers.w114 && Registers.w201)
    {
        Registers.w33 = true;
    }

    SB->SetSignal(BUS_WR, ESignalState::HiZ);

    if (!Registers.w33 && !Registers.w62)
    {
        SB->SetSignal(BUS_WR, ESignalState::High);
    }
    if (Registers.w33)
    {
        SB->SetSignal(BUS_WR, ESignalState::Low);
    }

    if (bClk)
    {
        if (Registers.w41 || Registers.w113)
        {
            SB->SetSignal(BUS_M1, ESignalState::High);
        }
        if (Registers.w131 && Registers.w110)
        {
            SB->SetSignal(BUS_M1, ESignalState::Low);
        }
    }

    Registers.w129 = !(Registers.w131 && (Registers.w109 || Registers.w41));

    if (bClk)
    {
        SB->SetSignal(BUS_RFSH, Registers.w129 ? ESignalState::High : ESignalState::Low);
    }
}

void FCPU_Z80_NMOS::BusLogic(bool bClk)
{
    Registers.w15 = !(!Registers.w114 || Registers.w202 || Registers.w201);

    if (bClk && Registers.w131 && Registers.w41)
    {
        Registers.w2 = 0;
    }
    Registers.w69 = !(Registers.w55 || (Registers.w41 && !Registers.w131));
    if (bClk)
    {
        Registers.l1 = Registers.w69;
    }
    if (!bClk && !Registers.l1)
    {
        Registers.w2 = false;
    }
    if (bClk && Registers.w15)
    {
        Registers.w2 = true;
    }
    if (bClk && Registers.w3 && Registers.w41)
    {
        Registers.w1 = true;
    }
    if (Registers.w55 || (bClk && (Registers.w114 || Registers.w201)))
    {
        Registers.w1 = false;
    }
    if (bClk)
    {
        Registers.l13 = Registers.w110 && Registers.w201;
    }

    Registers.w43 = !(!Registers.pla[35] && Registers.l13);
    Registers.w42 = !bClk && !Registers.w43;
    Registers.w45 = !bClk && !Registers.l15;

    if (Registers.w45)
    {
        Registers.w44 = false;
    }
    if (bClk)
    {
        Registers.l14 = Registers.w113;
    }
    if (Registers.l14 || (bClk && Registers.w110))
    {
        Registers.w44 = true;
    }
    if (Registers.w1)
    {
        Registers.bu1 = 0xFF;
        Registers.w146 = Registers.w145;
    }

    Registers.w483 = !Registers.w1 && Registers.w2;

    if (Registers.w483)
    {
        Registers.bu1 = 0xFF;
        Registers.w146 = 0xFF;
    }
}

void FCPU_Z80_NMOS::OpcodeFetch(bool bClk)
{
    // opcode fetch
    if (bClk)
    {
        Registers.l17 = !(Registers.w114 && Registers.w131);
        Registers.l18 = !(Registers.w41 || Registers.w55);
    }
    else
    {
        if (!Registers.l17)
        {
            Registers.w48 = true;
        }
        if (!Registers.l18)
        {
            Registers.w48 = false;
        }
    }

    Registers.w47 = !bClk && !Registers.l16;
    Registers.w49 = !(Registers.w48 || Registers.w47);

    if (!Registers.w49)
    {
        Registers.w147 = Registers.w146 ^ 0xFF;
    }
}

void FCPU_Z80_NMOS::HaltLogic(bool bClk)
{
    Registers.pla[3] = Registers.w147 == 0x76 && Registers.w90; // halt
    Registers.w11 = !(Registers.w12 || Registers.w9 || !Registers.pla[3]);
    Registers.w10 = !(Registers.w12 || Registers.w9 || Registers.w11);

    if (Registers.w11 && Registers.w16)
    {
        Registers.halt = true;
    }
    if (Registers.w19 || Registers.w18 || Registers.w55 || !Registers.w57)
    {
        Registers.halt = false;
    }

    SB->SetSignal(BUS_HALT, !Registers.halt ? ESignalState::High : ESignalState::Low);
}

void FCPU_Z80_NMOS::InterruptFlipFlops(bool bClk)
{
    Registers.w71 = !bClk && !Registers.l26;
    Registers.w75 = !bClk && !Registers.l27 && !Registers.w19;

    if (Registers.w18 || Registers.w55)
    {
        Registers.w74 = false;
    }
    else if (!bClk)
    {
        if (Registers.w71)
        {
            Registers.w74 = (Registers.w147 & 8) != 0;
        }
    }
    if (Registers.w19 || Registers.w18 || Registers.w55)
    {
        Registers.w73 = false;
    }
    else if (!bClk)
    {
        if (Registers.w71)
        {
            Registers.w73 = (Registers.w147 & 8) != 0;
        }
        if (Registers.w75)
        {
            Registers.w73 = Registers.w74;
        }
    }

    // interrupt mode
    Registers.w79 = !bClk && !Registers.l28;

    if (Registers.w55)
    {
        Registers.w78 = 1;
    }
    else if (!bClk)
    {
        if (Registers.w79)
        {
            Registers.w78 = (Registers.w147 & 8) == 0;
        }
    }
    if (Registers.w55)
    {
        Registers.w80 = 0;
    }
    else if (!bClk)
    {
        if (Registers.w79)
        {
            Registers.w80 = (Registers.w147 & 16) != 0;
        }
    }
}

void FCPU_Z80_NMOS::OpcodeDecode(bool bClk)
{
    // prefix logic
    Registers.w103 = !Registers.l29 && !bClk;

    Registers.w28 = !(Registers.halt || (Registers.w18 && Registers.w80) || Registers.w55 || Registers.w19 || !(Registers.w18 || Registers.l7));

    if (Registers.w55)
    {
        Registers.w30 = true;
    }
    else if (!bClk)
    {
        if (Registers.w103)
        {
            Registers.w30 = !Registers.w28;
        }
    }
    if (Registers.w55)
    {
        Registers.w95 = true;
    }
    else if (!bClk)
    {
        if (Registers.w103)
        {
            Registers.w95 = !Registers.w98;
        }
    }
    if (Registers.w55)
    {
        Registers.w92 = 0;
    }
    else if (!bClk)
    {
        if (Registers.w103)
        {
            Registers.w92 = !Registers.l43;
        }
    }
    if (Registers.w55)
    {
        Registers.w100 = 1;
    }
    else if (!bClk)
    {
        if (!Registers.w98 && Registers.w103)
        {
            Registers.w100 = Registers.w99;
        }
    }

    Registers.w89 = !(Registers.w103 || !Registers.w30);
    Registers.w77 = Registers.w89 && Registers.w19;
    Registers.w91 = !(Registers.w92 || !Registers.w95);
    Registers.w90 = !(!Registers.w91 || Registers.w30);
    Registers.w96 = Registers.w95 || Registers.w103;
    Registers.w287 = !Registers.w95 || (Registers.w103 && Registers.w98);

    // pla
    Registers.pla[33] = (Registers.w147 & 0xc0) == 0x80 && Registers.w90; // 0x80-0xbf alu opcodes?
    Registers.pla[34] = (Registers.w147 & 0xc7) == 0xc6 && Registers.w90; // n alu opcodes
    Registers.w82 = !(Registers.pla[33] || Registers.pla[34]);
    Registers.pla[0] = (Registers.w147 & 0xf7) == 0xd3 && Registers.w90; // out(n), a; in(n), a
    Registers.pla[1] = (Registers.w147 & 0xf7) == 0xf3 && Registers.w90; // di; ei
    Registers.pla[2] = (Registers.w147 & 0xc7) == 0x46 && Registers.w92; // im 0; im 1; im 2
    Registers.pla[4] = (Registers.w147 & 0xe7) == 0xa0 && Registers.w92; // ldi; ldd; ldir; lddr
    Registers.pla[5] = (Registers.w147 & 0xe7) == 0xa1 && Registers.w92; // cpi; cpd; cpir; cpdr
    Registers.pla[6] = Registers.w147 == 0x37 && Registers.w90; // scf
    Registers.pla[7] = (Registers.w147 & 0xe6) == 0xa2 && Registers.w92; // ini; outi; ind; outd; inir; otir; indr; otdr
    Registers.pla[8] = Registers.w147 == 0x10 && Registers.w90; // djnz d
    Registers.pla[9] = Registers.w147 == 0x3f && Registers.w90; // ccf
    Registers.pla[10] = (Registers.w147 & 0x38) == 0x28 && !Registers.w82; // xor
    Registers.pla[11] = (Registers.w147 & 0xf7) == 0x57 && Registers.w92; // ld a,i; ld a,r
    Registers.pla[12] = (Registers.w147 & 0x38) == 0x30 && !Registers.w82; // or
    Registers.pla[13] = (Registers.w147 & 0x38) == 0x20 && !Registers.w82; // and
    Registers.pla[14] = (Registers.w147 & 0x38) == 0x00 && !Registers.w82; // add
    Registers.pla[15] = (Registers.w147 & 0xf7) == 0x57 && Registers.w92 && Registers.w74; // ???
    Registers.pla[16] = (Registers.w147 & 0xc7) == 0x44 && Registers.w92; // neg
    Registers.pla[17] = Registers.w147 == 0x2f && Registers.w90; // cpl
    Registers.pla[18] = (Registers.w147 & 0x38) == 0x08 && !Registers.w82; // adc
    Registers.pla[19] = (Registers.w147 & 0x38) == 0x18 && !Registers.w82; // sbc
    Registers.pla[20] = (Registers.w147 & 0x38) == 0x10 && !Registers.w82; // sub
    Registers.pla[21] = Registers.w147 == 0x27 && Registers.w90; // daa
    Registers.pla[22] = (Registers.w147 & 0x38) == 0x38 && !Registers.w82; // cp
    Registers.pla[23] = (Registers.w147 & 0xc7) == 0x05 && Registers.w90; // dec byte
    Registers.pla[24] = (Registers.w147 & 0xc0) == 0xc0 && !Registers.w96; // set
    Registers.pla[25] = (Registers.w147 & 0xc0) == 0x80 && !Registers.w96; // res
    Registers.pla[26] = (Registers.w147 & 0xc0) == 0x40 && !Registers.w96; // bit
    Registers.pla[27] = (Registers.w147 & 0xe7) == 0x07 && Registers.w90; // rlca; rrca; rla; rra
    Registers.pla[28] = (Registers.w147 & 0xc0) == 0x00 && !Registers.w96; // rlc; rrc; rl; rr; sla; sra; sll; srl
    Registers.pla[29] = (Registers.w147 & 0xcf) == 0x09 && Registers.w90; // add hl, bc; de ; hl ;sp
    Registers.pla[30] = (Registers.w147 & 0xc7) == 0x42 && Registers.w92; // sbc hl, adc hl
    Registers.pla[31] = (Registers.w147 & 0xc7) == 0x40 && Registers.w92; // in (c)
    Registers.pla[32] = (Registers.w147 & 0xc6) == 0x04 && Registers.w90; // inc dec byte
    Registers.pla[35] = (Registers.w147 & 0xc7) == 0x06 && Registers.w90; // ld n opcodes
    Registers.pla[36] = !Registers.w96;
    Registers.pla[37] = (Registers.w147 & 0xc0) == 0x40 && Registers.w90; // ld reg opcodes
    Registers.pla[38] = (Registers.w147 & 0xf7) == 0x67 && Registers.w92; // rrd, rld
    Registers.pla[39] = (Registers.w147 & 0xf8) == 0x70 && Registers.w90 && !Registers.pla[3]; // ld to (hl) opcodes
    Registers.pla[40] = (Registers.w147 & 0xc7) == 0x46 && Registers.w90 && !Registers.pla[3]; // ld from (hl) opcodes
    Registers.pla[41] = (Registers.w147 & 0xf7) == 0x47 && Registers.w92; // ld i,a ; ld r,a
    Registers.pla[42] = (Registers.w147 & 0xc7) == 0xc7 && Registers.w90; // rst n
    Registers.pla[43] = (Registers.w147 & 0x07) == 0x06 && !Registers.w96; // bit opcode (hl)
    Registers.pla[44] = !Registers.w96 && !Registers.w100; // 
    Registers.pla[45] = (Registers.w147 & 0xfe) == 0x34 && Registers.w90; // inc dec (hl)
    Registers.pla[46] = (Registers.w147 & 0xc7) == 0x86 && Registers.w90; // alu (hl)
    Registers.pla[47] = Registers.w147 == 0xed && Registers.w90; // misc opcode prefix
    Registers.pla[48] = Registers.w147 == 0x36 && Registers.w90; // ld (hl), n
    Registers.pla[49] = Registers.w147 == 0xcb && !Registers.w100; // ix, iy bit instutruction ?
    Registers.pla[50] = (Registers.w147 & 0xe7) == 0x20 && Registers.w90; // jr nz, z, nc, c
    Registers.pla[51] = Registers.w147 == 0x18 && Registers.w90; // jr d
    Registers.pla[52] = (Registers.w147 & 0xc7) == 0x45 && Registers.w92; // retn, reti
    Registers.pla[53] = (Registers.w147 & 0xc7) == 0xc0 && Registers.w90; // ret condition
    Registers.pla[54] = Registers.w147 == 0xcb && Registers.w90; // bit opcode prefix
    Registers.pla[55] = (Registers.w147 & 0xc7) == 0xc2 && Registers.w90; // jp n condition
    Registers.pla[56] = (Registers.w147 & 0xc7) == 0xc4 && Registers.w90; // call n condition
    Registers.pla[57] = (Registers.w147 & 0xdf) == 0xdd && Registers.w90; // ix, iy
    Registers.pla[58] = Registers.w147 == 0x36 && Registers.w90 && !Registers.w100; // ld (ix/y), n
    Registers.pla[59] = Registers.w147 == 0x08 && Registers.w90; // ex af, af'
    Registers.pla[60] = (Registers.w147 & 0xf7) == 0x32 && Registers.w90; // ld (nn), a; ld a, (nn)
    Registers.pla[61] = (Registers.w147 & 0xf7) == 0xd3 && Registers.w90; // out (n), a; in a, (n)
    Registers.pla[62] = (Registers.w147 & 0xe7) == 0x02 && Registers.w90; // ld (bc), a; ld (de), a; ld a, (bc); ld a(de)
    Registers.pla[63] = Registers.w147 == 0xc9 && Registers.w90; // ret
    Registers.pla[64] = (Registers.w147 & 0xc7) == 0x41 && Registers.w92; // out (c), reg
    Registers.pla[65] = (Registers.w147 & 0xcf) == 0x43 && Registers.w92; // ld (nn), word reg
    Registers.pla[66] = (Registers.w147 & 0xe7) == 0x47 && Registers.w92; // ld i, a; ld r, a; ld a, i; ld a, r
    Registers.pla[67] = (Registers.w147 & 0xc7) == 0x43 && Registers.w92; // ld (nn), word reg, ld word reg, (nn)
    Registers.pla[68] = (Registers.w147 & 0xf7) == 0x22 && Registers.w90; // ld (nn), hl; ld hl, (nn)
    Registers.pla[69] = Registers.w147 == 0xc3 && Registers.w90; // jp nn
    Registers.pla[70] = Registers.w147 == 0xd3 && Registers.w90; // out (n), a
    Registers.pla[71] = (Registers.w147 & 0xc6) == 0x40 && Registers.w92; // in/ out (c), byte
    Registers.pla[72] = Registers.w147 == 0x10 && Registers.w90; // djnz d
    Registers.pla[73] = (Registers.w147 & 0xe7) == 0x07 && Registers.w90; // rlca; rrca; rla; rra
    Registers.pla[74] = Registers.w147 == 0xcd && Registers.w90; // call nn
    Registers.pla[75] = (Registers.w147 & 0xcb) == 0xc1 && Registers.w90; // pop, push
    Registers.pla[76] = Registers.w147 == 0xcb && !Registers.w100; // ix, iy bit instutruction ?
    Registers.pla[77] = (Registers.w147 & 0xe7) == 0xa2 && Registers.w92; // ini; ind; inir; indr
    Registers.pla[78] = (Registers.w147 & 0xe7) == 0xa3 && Registers.w92; // outi; outd; otir; otdr
    Registers.pla[79] = (Registers.w147 & 0xe7) == 0xa1 && Registers.w92; // cpi; cpd; cpir; cpdr
    Registers.pla[80] = (Registers.w147 & 0xe7) == 0xa0 && Registers.w92; // ldi; ldd; ldir; lddr
    Registers.pla[81] = (Registers.w147 & 0xc7) == 0x06 && Registers.w90; // ld byte n
    Registers.pla[82] = (Registers.w147 & 0xcf) == 0xc5 && Registers.w90; // push
    Registers.pla[83] = (Registers.w147 & 0xf7) == 0x67 && Registers.w92; // rrd, rld
    Registers.pla[84] = (Registers.w147 & 0xcf) == 0x0b && Registers.w90; // dec word
    Registers.pla[85] = (Registers.w147 & 0xcf) == 0x02 && Registers.w90; // load from address
    Registers.pla[86] = (Registers.w147 & 0xe7) == 0xa0 && Registers.w92; // ldi; ldd; ldir; lddr
    Registers.pla[87] = (Registers.w147 & 0xe7) == 0xa1 && Registers.w92; // cpi; cpd; cpir; cpdr
    Registers.pla[88] = Registers.w147 == 0xe3 && Registers.w90; // ex (sp), hl
    Registers.pla[89] = (Registers.w147 & 0xc7) == 0x03 && Registers.w90; // inc, dec word
    Registers.pla[90] = (Registers.w147 & 0xe7) == 0x02 && Registers.w90; // ld address from register
    Registers.pla[91] = (Registers.w147 & 0xcf) == 0x01 && Registers.w90; // ld nn word
    Registers.pla[92] = Registers.w147 == 0xe9 && Registers.w90; // jp (hl)
    Registers.pla[93] = Registers.w147 == 0xf9 && Registers.w90; // ld sp, hl
    Registers.pla[94] = (Registers.w147 & 0xe7) == 0x47 && Registers.w92; // ld i,a; ld r,a; ld a,i; ld a,r
    Registers.pla[95] = (Registers.w147 & 0xdf) == 0xdd && Registers.w90; // ix, iy
    Registers.pla[96] = Registers.w147 == 0xeb && Registers.w90; // ex de, hl
    Registers.pla[97] = Registers.w147 == 0xd9 && Registers.w90; // exx
    Registers.pla[98] = (Registers.w147 & 0xf4) == 0xa0 && Registers.w92; // 

    Registers.w97 = !(Registers.pla[47] || Registers.pla[54] || Registers.pla[57]);

    if (bClk)
    {
        Registers.w104 = Registers.w97;
    }

    Registers.w57 = Registers.w56 || !Registers.w104;

    if (bClk)
    {
        Registers.l7 = Registers.w57;
    }
    if (bClk)
    {
        Registers.w107 = Registers.pla[76];
    }
    if (bClk)
    {
        Registers.l16 = !(Registers.w107 && Registers.w127 && Registers.w41);
    }

    Registers.w102 = !((Registers.w131 && Registers.w114) || (Registers.w110 && Registers.w127 && Registers.w107));

    if (bClk)
    {
        Registers.l29 = Registers.w102;
    }
    if (bClk)
    {
        Registers.w98 = Registers.pla[54];
    }
    if (bClk)
    {
        Registers.l43 = !Registers.pla[47];
    }
    if (bClk)
    {
        Registers.w99 = !Registers.pla[57];
    }

    Registers.w87 = !(Registers.w78 || !Registers.w80);
    Registers.w85 = !(Registers.w78 && Registers.w18);
    Registers.w84 = !(!Registers.w80 || Registers.w85);
    Registers.w86 = (Registers.w89 && (Registers.w84 || Registers.w19)) || (!Registers.w89 && Registers.pla[42]);
    Registers.w88 = !(Registers.w87 && Registers.w89 && Registers.w18);

    Registers.w148 = !(Registers.pla[11] || Registers.pla[16] || Registers.pla[17] ||
        Registers.pla[21] || Registers.pla[27] || Registers.pla[33] || Registers.pla[34]
        || Registers.pla[38]);
    Registers.w149 = !(Registers.w86 || !Registers.w88 || Registers.pla[53] || Registers.pla[72]
        || Registers.pla[77] || Registers.pla[78] || Registers.pla[82] || Registers.pla[89]
        || Registers.pla[93] || Registers.pla[94]);
    Registers.w150 = !(Registers.pla[11] || Registers.pla[21] || Registers.pla[27]
        || Registers.pla[28] || Registers.pla[31] || Registers.pla[33] || Registers.pla[34]
        || Registers.pla[35] || Registers.pla[37]);
    Registers.w151 = !(!Registers.w88 || Registers.pla[55] || Registers.pla[60]
        || Registers.pla[67] || Registers.pla[68] || Registers.pla[69] || Registers.pla[77]
        || Registers.pla[78] || Registers.pla[91]);
    Registers.w152 = !(Registers.pla[4] || Registers.pla[5] || Registers.pla[6] || Registers.pla[7]
        || Registers.pla[9] || Registers.pla[26] || Registers.pla[28] || Registers.pla[29]
        || Registers.pla[30] || Registers.pla[31] || Registers.pla[32] || Registers.pla[33]
        || Registers.pla[34]);
    Registers.w153 = !(Registers.w86 || !Registers.w88 || Registers.pla[50] || Registers.pla[51]
        || Registers.pla[52] || Registers.pla[53] || Registers.pla[55] || Registers.pla[56]
        || Registers.pla[63] || Registers.pla[69] || Registers.pla[72] || Registers.pla[74]
        || Registers.pla[92]);
    Registers.w154 = !(Registers.pla[11] || Registers.pla[16] || Registers.pla[17]
        || Registers.pla[27] || Registers.pla[28] || Registers.pla[31] || Registers.pla[35]
        || Registers.pla[37] || Registers.w86);
    Registers.w155 = !(!Registers.w88 || Registers.pla[44] || Registers.pla[45]
        || Registers.pla[49] || Registers.pla[56] || Registers.pla[60] || Registers.pla[67]
        || Registers.pla[68] || Registers.pla[74] || Registers.pla[77] || Registers.pla[78]
        || Registers.pla[83] || Registers.pla[88]);
    Registers.w156 = !(Registers.pla[10] || Registers.pla[12] || Registers.pla[14]
        || Registers.pla[16] || Registers.pla[18] || Registers.pla[19] || Registers.pla[20]
        || Registers.pla[22] || Registers.pla[29] || Registers.pla[30]);
    Registers.w157 = !(!Registers.w88 || Registers.pla[50] || Registers.pla[51]
        || Registers.pla[55] || Registers.pla[56] || Registers.pla[60] || Registers.pla[67]
        || Registers.pla[68] || Registers.pla[69] || Registers.pla[72] || Registers.pla[74]
        || Registers.pla[77] || Registers.pla[78] || Registers.pla[79] || Registers.pla[80]
        || Registers.pla[83] || Registers.pla[88] || Registers.pla[91]);
    Registers.w158 = !(Registers.pla[11] || Registers.pla[14] || Registers.pla[16]
        || Registers.pla[18] || Registers.pla[19] || Registers.pla[20] || Registers.pla[22]
        || Registers.pla[30] || Registers.pla[32]);
    Registers.w159 = !(Registers.pla[34] || Registers.pla[35] || Registers.pla[50]
        || Registers.pla[51] || Registers.pla[61] || Registers.pla[72]);
    Registers.w160 = !(Registers.pla[5] || Registers.pla[7] || Registers.pla[8]
        || Registers.pla[16] || Registers.pla[17] || Registers.pla[19] || Registers.pla[20]
        || Registers.pla[22] || Registers.pla[23] || Registers.pla[25]);
    Registers.w161 = !(Registers.pla[29] || Registers.pla[30] || Registers.w86
        || Registers.pla[48] || Registers.pla[52] || Registers.pla[53] || Registers.pla[61]
        || Registers.pla[62] || Registers.pla[63] || Registers.pla[71] || Registers.pla[75]);
    Registers.w162 = !(Registers.pla[5] || Registers.pla[7] || Registers.pla[8]
        || Registers.pla[11] || Registers.pla[16] || Registers.pla[21] || Registers.pla[26]
        || Registers.pla[28] || Registers.pla[30] || Registers.pla[31] || Registers.pla[32]
        || Registers.pla[33] || Registers.pla[34] || Registers.pla[38]);
    Registers.w163 = !(Registers.pla[32] || Registers.pla[33] || Registers.pla[34]
        || Registers.pla[36] || Registers.pla[37]);
    Registers.w164 = !(Registers.pla[26] || Registers.pla[39] || Registers.pla[40]
        || Registers.pla[46] || Registers.pla[48] || Registers.pla[60] || Registers.pla[61]
        || Registers.pla[62] || Registers.pla[71] || Registers.pla[77] || Registers.pla[78]
        || Registers.pla[79] || Registers.pla[80] || Registers.pla[83]);
    Registers.w165 = !(Registers.pla[7] || Registers.pla[8] || Registers.pla[13]
        || Registers.pla[17] || Registers.pla[26] || Registers.pla[32]);
    Registers.w166 = !(Registers.pla[9] || Registers.pla[18] || Registers.pla[19]
        || Registers.pla[30]);
    Registers.w167 = !(Registers.w86 || Registers.pla[39] || Registers.pla[48]
        || Registers.pla[56] || Registers.pla[64] || Registers.pla[65] || Registers.pla[70]
        || Registers.pla[74] || Registers.pla[82] || Registers.pla[83] || Registers.pla[85]
        || Registers.pla[88] || Registers.pla[89] || Registers.pla[93]);
    Registers.w168 = !(Registers.pla[10] || Registers.pla[12] || Registers.pla[24]);
    Registers.w169 = !(Registers.pla[39] || Registers.pla[40] || Registers.pla[43]
        || Registers.pla[44] || Registers.pla[45] || Registers.pla[46] || Registers.pla[48]
        || Registers.pla[49]);
    Registers.w170 = !(Registers.pla[49] || Registers.pla[55] || Registers.pla[56] || Registers.pla[44]
        || Registers.pla[58] || Registers.pla[60] || Registers.pla[67] || Registers.pla[68]
        || Registers.pla[69] || Registers.pla[74] || Registers.pla[91]);
    Registers.w171 = !(Registers.pla[6] || Registers.pla[9] || Registers.pla[13]);
    Registers.w172 = !(Registers.pla[24] || Registers.pla[25] || Registers.pla[28]
        || Registers.pla[31] || Registers.pla[32] || Registers.pla[35] || Registers.pla[37]);
    Registers.w173 = !(Registers.w86 || Registers.pla[52] || Registers.pla[53] || Registers.pla[56]
        || Registers.pla[63] || Registers.pla[74] || Registers.pla[75] || Registers.pla[88]);
    Registers.w174 = !(Registers.pla[7] || Registers.pla[8] || Registers.pla[32]
        || Registers.pla[36] || Registers.pla[50] || Registers.pla[51]);
    Registers.w175 = !(Registers.pla[50] || Registers.pla[51] || Registers.pla[72]);
    Registers.w176 = !(Registers.pla[82] || Registers.pla[84]);
    Registers.w177 = !(Registers.pla[4] || Registers.pla[5] || Registers.pla[6]
        || Registers.pla[7] || Registers.pla[9]);
    Registers.w178 = !(Registers.pla[7] || Registers.pla[8]);
    Registers.w179 = !(Registers.pla[5] || Registers.pla[7] || Registers.pla[8]);
    Registers.w180 = !(Registers.pla[12] || Registers.pla[24]);
    Registers.w181 = !(Registers.pla[13] || Registers.pla[25] || Registers.pla[26]);
    Registers.w182 = !(Registers.pla[33] || Registers.pla[36]);
    Registers.w183 = !(Registers.w182 && (Registers.w114 || !Registers.pla[37]));
    Registers.w184 = !(Registers.pla[55] || Registers.pla[56]);
    Registers.w185 = !(Registers.pla[56] || Registers.pla[74]);
    Registers.w186 = !(Registers.pla[77] || Registers.pla[78] || Registers.pla[79]
        || Registers.pla[80]);
    Registers.w187 = !(Registers.pla[60] || Registers.pla[61] || Registers.pla[62]);
    Registers.w188 = !(Registers.pla[71] || Registers.pla[72] || Registers.pla[77]
        || Registers.pla[78]);
    Registers.w189 = !(Registers.pla[72] || Registers.pla[73] || Registers.pla[77]
        || Registers.pla[78]);
    Registers.w190 = !(Registers.pla[89] || Registers.pla[93]);
    Registers.w191 = !(Registers.pla[79] || Registers.pla[80] || Registers.pla[81]
        || !Registers.w169 || Registers.pla[83] || Registers.pla[92] || Registers.pla[93]);

    Registers.w256 = !Registers.w169 && !Registers.w100;

    Registers.w255 = !Registers.w174 || (Registers.w115 && Registers.w256);

    Registers.w254 = !(!Registers.w255 || !Registers.w186);

    Registers.w196 = Registers.pla[86] || Registers.pla[87];
    Registers.w195 = !(Registers.pla[88] || !(Registers.w196 || (!Registers.w299 && !Registers.w173)));

    Registers.w197 = !Registers.w234 && Registers.w186;
    Registers.w199 = !(Registers.pla[83] || Registers.pla[87] || Registers.w254);
    Registers.w198 = !Registers.w199 && Registers.w170;

    Registers.w203 = !(Registers.w110 || (Registers.w41 && Registers.w131));

    Registers.w204 = !((Registers.w109 && Registers.pla[93])
        || (Registers.pla[88] && Registers.w121 && Registers.w41));

    if (bClk)
    {
        Registers.l39 = Registers.w204;
    }
    else
    {
        Registers.w205 = !Registers.l39;
    }

    Registers.w200 = !((Registers.w127 && (!Registers.w186 || !Registers.w88))
        || (Registers.w120 && !Registers.w88)
        || (Registers.w123 && !Registers.w167)
        || (Registers.w121 && (!Registers.w167 || Registers.w255)));

    Registers.w202 = ((Registers.w121 || Registers.w123) && !Registers.w197)
        || (Registers.w127 && Registers.w198);
    Registers.w201 = !Registers.w200 && !Registers.w202;
    Registers.w3 = !(Registers.w201 || Registers.w202);

    Registers.w207 = !(Registers.w176 && !(Registers.w86 || !Registers.w88));
    Registers.w206 = !((Registers.w131 && Registers.w109 && Registers.w207)
        || (!Registers.w186 && Registers.w123 && (Registers.w41 || Registers.w110))
        || (Registers.w195 && Registers.w127 && Registers.w41));

    Registers.w209 = !Registers.w186 && (Registers.w147 & 8) != 0;
    Registers.w208 = !(Registers.w110 &&
        ((Registers.w120 && (Registers.w209 || !Registers.w88))
            || (Registers.w123 && !Registers.w173 && !Registers.w167)
            || (Registers.w127 && Registers.w209)));

    if (bClk)
    {
        Registers.l40 = !Registers.w206 || !Registers.w208;
    }
    else
    {
        Registers.w210 = Registers.l40;
    }

    Registers.w135 = !((Registers.w190 && Registers.w68) || (Registers.w131 && Registers.w109 && Registers.w149));
    Registers.w136 = !(Registers.w61 || (Registers.w41 && Registers.w143) || (Registers.w109 && Registers.w141));
    Registers.w137 = !Registers.w135 || !Registers.w136;
    Registers.w133 = !Registers.w137 && !Registers.w55;

    if (bClk)
    {
        Registers.l36 = !Registers.w133;
    }
    if (bClk)
    {
        Registers.l24 = !Registers.w63 && !Registers.w133;
    }
    if (bClk)
    {
        Registers.w112 = !Registers.w113 && Registers.w133;
    }

    Registers.w105 = Registers.pla[61] || Registers.pla[71];
    Registers.w106 = (Registers.pla[77] && Registers.w120) || (Registers.w127 && Registers.pla[78]) || (Registers.w105 && Registers.w123);

    Registers.w93 = !(Registers.w131 || Registers.w106);
    Registers.w27 = !((Registers.w110 && Registers.w93) || (Registers.w131 && (Registers.w41 || (Registers.w110 && !Registers.w18))));

    if (bClk)
    {
        Registers.l6 = Registers.w27;
    }

    Registers.w101 = !(Registers.w202 || Registers.w201 || (Registers.w131 && (Registers.w41 || Registers.w18)));
    
    if (bClk)
    {
        Registers.l8 = Registers.w101;
    }
    if (bClk)
    {
        Registers.w38 = !(((Registers.w18 && Registers.w131) || Registers.w106) && (!Registers.w37 || Registers.w114));
    }
    if (bClk)
    {
        Registers.w39 = Registers.w38 && !SB->IsActive(BUS_WAIT);
    }
    if (bClk)
    {
        Registers.l15 = !(Registers.w201 && Registers.w110);
    }

    Registers.w46 = !(Registers.w131 || (Registers.w127 && Registers.pla[35]) || (Registers.w127 && Registers.w107));
    Registers.w192 = (Registers.w201 && Registers.w110) || (Registers.w41 && Registers.w3 && Registers.w46);

    Registers.w138 = Registers.w161 && !Registers.w126;
    Registers.w139 = Registers.w159 && Registers.w161 && Registers.w169 && Registers.w157;
    Registers.w117 = Registers.w55 || Registers.w121 || (Registers.w123 && !Registers.w164) || (Registers.w120 && Registers.w138 && !Registers.w159)
        || (Registers.w127 && (Registers.pla[98] || (Registers.w155 && (!(!Registers.w164 || !Registers.w151) || !Registers.w151))));
    Registers.w118 = Registers.w117 || Registers.w299 || (Registers.w131 && Registers.w139);
    
    if (bClk)
    {
        Registers.w130 = Registers.w118;
    }
    if (bClk)
    {
        Registers.w134 = Registers.w125 && ((Registers.w159 && Registers.w131) || Registers.w120);
    }

    Registers.w125 = !((!Registers.w169 && !Registers.w100) || (Registers.w169 && Registers.w161));
    Registers.w126 = (!Registers.w169 && !Registers.w100) || Registers.w255;

    Registers.w140 = !Registers.w155 && Registers.w151 && !Registers.w255;
    Registers.w141 = Registers.w186 && ((Registers.w140 && Registers.w127) || Registers.w123);
    Registers.w142 = Registers.w186 && !Registers.w255 && !Registers.w234;

    Registers.w143 = Registers.w120 || (Registers.w142 && Registers.w123) || (Registers.w121 && !Registers.pla[88])
        || (Registers.w127 && (!Registers.w151 || (Registers.w140 && Registers.w299)));
    Registers.w212 = !(Registers.w186 && (!Registers.w169 || Registers.w255));
    Registers.w213 = !(Registers.w186 && Registers.w218);

    Registers.w211 = !((Registers.w114 && ((Registers.w123 && Registers.w212)
        || (Registers.w120 && Registers.w213)))
        || (Registers.w109 && (Registers.w121 || ((Registers.w123 || Registers.w127)
            && (!Registers.w186 || !Registers.w173)))));

    Registers.w214 = !((Registers.w41 && Registers.w131)
        || (Registers.w68 && Registers.w131 && (!Registers.w88 || !Registers.w167))
        || (Registers.w114 && ((Registers.w127 && !Registers.w186)
            || (Registers.w121 && Registers.w167 && !Registers.w173))));

    if (bClk)
    {
        Registers.l41 = Registers.w55 || (!Registers.w57 && Registers.w110 && Registers.w131);
    }

    Registers.w215 = Registers.l41;

    Registers.w216 = !Registers.w214 || !Registers.w211;

    Registers.w218 = !Registers.pla[88] && Registers.w88;
    Registers.w217 = !((Registers.w120 && Registers.w218) || Registers.w131 || Registers.w127);

    Registers.w219 = !((Registers.w41 && (Registers.w121 || (Registers.w127 && !Registers.w186)))
        || (Registers.w109 && Registers.w131)
        || (Registers.w114 && (Registers.w131 || (Registers.w127 && !Registers.w173))));

    Registers.w220 = !(Registers.w127 && Registers.w196);
    Registers.w221 = !((Registers.w110 && (Registers.w123 || Registers.w121)
        && (!Registers.w185 || Registers.w86))
        || (Registers.w109 && Registers.w123 && !Registers.w186));

    Registers.w222 = !((!Registers.w175 && ((Registers.w114 && Registers.w120)
        || (Registers.w41 && Registers.w127)))
        || (Registers.w110 && (Registers.w127 || Registers.w120) && !Registers.w88));

    Registers.w294 = !(Registers.w221 && Registers.w222);

    Registers.w225 = !Registers.w100 && !Registers.w169;

    Registers.w224 = !Registers.w170 || Registers.w225 || !Registers.w159;

    Registers.w223 = !((Registers.w127 && !Registers.w186)
        || (Registers.w120 && !Registers.w170)
        || (Registers.w131 && Registers.w224));

    Registers.w226 = !((Registers.w110 && (
        Registers.w131
        || (Registers.w120 && Registers.w224)
        || (Registers.w127 && !Registers.w170)))
        || Registers.w55);


    Registers.w228 = !(Registers.w131 && Registers.w41);
    Registers.w229 = !((Registers.w41 && Registers.w120 && !Registers.w88)
        || (Registers.w109 && Registers.w131 && Registers.pla[94]));
    Registers.w230 = !((Registers.w68 && Registers.w131 && (Registers.pla[93] || !Registers.w173))
        || (Registers.w114 && (((Registers.w127 || Registers.w123) && !Registers.w173)
            || (Registers.w121 && !Registers.w173 && Registers.w167))));
    Registers.w231 = !(((Registers.w68 || Registers.w109) && (Registers.w131 || Registers.w127)
        && (!Registers.w173 || !Registers.w88))
        || (Registers.w114 && Registers.w120 && !Registers.w88));
    Registers.w232 = !((Registers.w41 && (Registers.w120 || Registers.w127) && Registers.pla[91])
        || ((Registers.w109 || Registers.w68)
            && (Registers.w121 || (Registers.w131
                && (Registers.pla[89] || Registers.pla[90]))))
        );

    Registers.w234 = Registers.pla[29] || Registers.pla[30];

    Registers.w236 = !(Registers.pla[67] || Registers.pla[68]);
    Registers.w235 = !(Registers.w236 || (Registers.w147 & 8) != 0);
    Registers.w237 = !(Registers.w236 || !Registers.w167);

    Registers.w233 = !(
        (Registers.w110 && (Registers.w123 || Registers.w121)
            && (Registers.pla[88] || Registers.w234 || Registers.w235))
        || (Registers.w41 && (Registers.w123 || Registers.w121)
            && Registers.w237)
        );

    Registers.w238 = !(
        ((Registers.w41 && Registers.w127) || (Registers.w114 && Registers.w120))
        && Registers.w225);

    Registers.w240 = !(Registers.pla[77] || Registers.w186);

    Registers.w239 = !((Registers.w114 && Registers.w120 && Registers.w240)
        || (Registers.w109 && ((Registers.w127 && Registers.pla[83]) || (Registers.w123 && Registers.w234)))
        || (Registers.w41 && (Registers.w123 || Registers.w121) && Registers.w234)
        );

    Registers.w241 = !(
        (Registers.w131 && Registers.w68 && Registers.pla[78])
        || (Registers.w131 && Registers.w109 && (Registers.w234 || !Registers.w191))
        || (Registers.w114 && Registers.w127 && Registers.pla[77])
        || (Registers.w41 && Registers.w120 && (Registers.pla[77] || Registers.pla[81]))
        );

    Registers.w242 = !(
        ((Registers.w41 && Registers.w120)
            || (Registers.w114 && Registers.w127)) && Registers.pla[80]
        );

    Registers.w243 = !(
        (Registers.w109 && ((Registers.w127 && !Registers.w186) || (Registers.w131 && !Registers.w188)))
        || (Registers.w68 && Registers.w131 && Registers.pla[77])
        || (Registers.w110 && Registers.w120 && !Registers.w188)
        || (Registers.w41 && ((Registers.w127 && !Registers.w186) || (Registers.w120 && Registers.pla[78])))
        );

    Registers.w244 = !(
        (Registers.w110 || Registers.w41) && (Registers.w123 || Registers.w121) && Registers.pla[75]
        );

    Registers.w245 = !(!Registers.w187 || (!Registers.w236 && (Registers.w147 & 8) != 0)
        || (!Registers.w153 && Registers.w175) || Registers.pla[75] || !Registers.w173
        || Registers.pla[91]);

    Registers.w247 = !(Registers.pla[22] || Registers.w148);

    Registers.w246 = !((Registers.w110 && Registers.w123 && !Registers.w187)
        || (Registers.w41 && Registers.w123 && Registers.w167 && !Registers.w187)
        || (Registers.w109 && Registers.w131 && Registers.pla[73])
        || (Registers.w114 && Registers.w131 && Registers.w247)
        );

    Registers.w248 = !(
        (Registers.w110 && Registers.w123 && (Registers.pla[71] || !Registers.w163))
        || (Registers.w109 && Registers.w131 && (!Registers.w163 && Registers.w169))
        );

    Registers.w249 = !(Registers.w109 && Registers.w131 && Registers.pla[66]);

    Registers.w250 = !(Registers.w114 && Registers.w131 && !Registers.w172);

    Registers.w252 = Registers.w187 && (Registers.w86 || Registers.w234 || (Registers.w167 && !Registers.w245));

    Registers.w253 = !Registers.w245 || Registers.w235;

    Registers.w251 = !(
        (Registers.w41 && ((Registers.w123 && Registers.w252)
            || (Registers.w120 && Registers.w88 && Registers.w253)))
        || (Registers.w114 && Registers.w127 && (!Registers.w88 || Registers.w254))
        );

    Registers.w257 = !(
        (Registers.w110 && Registers.w120 && (!Registers.w189 || !Registers.w187))
        || (Registers.w41 && ((Registers.w127 && Registers.w253) || (Registers.w121 && Registers.w252)))
        );

    Registers.w258 = !(
        (Registers.w41 && Registers.w123 && !Registers.w187)
        || (Registers.w68 && Registers.w127 && Registers.w255)
        || (Registers.w114 && Registers.w131 && Registers.w247)
        );

    Registers.w259 = !(Registers.w110 && Registers.w131 && Registers.pla[59]);  // ex af, af'
    Registers.w261 = Registers.w235 || Registers.w234;

    Registers.w260 = !(
        (Registers.w110 && Registers.w121 && !Registers.w245)
        || (Registers.w109 && Registers.w131 && Registers.w261)
        );

    Registers.w262 = !(
        (Registers.w110 && ((Registers.w120 && !Registers.w88) || (Registers.w123 && Registers.w261)))
        || (Registers.w114 && Registers.w120)
        );

    Registers.w263 = !(
        (Registers.w109 && ((Registers.w131 && !Registers.w189) || Registers.w123))
        || ((Registers.w110 || Registers.w41) && Registers.w120 && !Registers.w88)
        );

    Registers.w264 = !(
        (Registers.w110 && ((Registers.w123 && (!Registers.w245 || !Registers.w187))
            || (Registers.w121 && Registers.w261)))
        || (Registers.w41 && Registers.w127 && Registers.w255)
        );

    Registers.w265 = (
        (Registers.w127 && !Registers.w184)
        || (Registers.w131 && Registers.pla[53])
        || (Registers.w120 && Registers.pla[50])
        );

    Registers.w266 = !(
        (Registers.w109 && Registers.w131 && Registers.pla[41])
        || (Registers.w110 && Registers.w127 && Registers.w88)
        );

    Registers.w267 = !(
        (Registers.w110 && Registers.w123 && Registers.pla[38])
        || (Registers.w41 && ((Registers.w123 && Registers.w86)
            || (Registers.w127 && Registers.pla[38])))
        );
    if (bClk)
        Registers.w378 = !Registers.w267 || !Registers.w266;

    Registers.w268 = !(!Registers.w150 &&
        ((Registers.w41 && Registers.w123)
            || (Registers.w109 && Registers.w131))
        );

    Registers.w269 = !(
        (Registers.w110 && ((Registers.w123 && (Registers.w256 || Registers.w234)) || (Registers.w121 && Registers.w234)))
        || (Registers.w68 && Registers.w131 && Registers.w86)
        || (Registers.w41 && (Registers.w131 || Registers.w120))
        );

    if (bClk)
    {
        Registers.w379 = !(!Registers.w268 || !Registers.w269);
    }

    Registers.w492 = !bClk && !Registers.w379;

    Registers.w271 = Registers.pla[27] || Registers.pla[28];

    Registers.w270 = !(
        (Registers.w41 && Registers.w123 && (Registers.w256 || Registers.w271))
        || (Registers.w109 && Registers.w131)
        );

    if (bClk)
    {
        Registers.w390 = !Registers.w270;
    }

    Registers.w389 = !(Registers.w390 && !Registers.w162);
    Registers.w436 = !bClk && !Registers.w389;

    Registers.w272 = !((Registers.w41 && Registers.w120 && Registers.w255));
    
    if (bClk)
    {
        Registers.l71 = Registers.w272;
    }
    
    Registers.w465 = !bClk && !Registers.l71;

    Registers.w273 = !(Registers.w110 && Registers.w131 && (Registers.w247 || !Registers.w152));

    Registers.w274 = !(
        (Registers.w114 && Registers.w123 && (Registers.w256 || Registers.w271))
        || (Registers.w41 && Registers.w131)
        );

    if (bClk)
    {
        Registers.l52 = !Registers.w274;
    }
    if (bClk)
    {
        Registers.l50 = Registers.w273;
        Registers.l51 = Registers.w274;
    }
    else
    {
        if (!Registers.l50)
            Registers.w380 = 0;
        if (!Registers.l51)
            Registers.w380 = 1;
    }
    if (bClk)
    {
        Registers.w381 = !Registers.w380 && !Registers.w274;
    }

    Registers.w382 = !bClk && Registers.l52 && !Registers.w381;

    if (bClk)
    {
        Registers.w419 = Registers.w274;
    }

    Registers.w275 = !(
        (Registers.w114 && Registers.w131 && (!Registers.w172 || Registers.w247))
        || (Registers.w110 && (Registers.w120 || Registers.w121) && Registers.w255)
        || (Registers.w41 && Registers.w121 && Registers.w234)
        || (Registers.w68 && Registers.w127)
        );

    Registers.w276 = !(Registers.w41 && Registers.w120 && Registers.w255);
    
    if (bClk)
    {
        Registers.l53 = !Registers.w276;
    }

    Registers.w277 = !(Registers.w114 && Registers.w131 && !Registers.w156);
    Registers.w278 = !(Registers.w114 && Registers.w131 && !Registers.w171);
    
    if (bClk)
    {
        Registers.l70 = Registers.w278;
    }

    Registers.w463 = !bClk && !Registers.l70;

    Registers.w279 = !(
        (Registers.w120 && Registers.pla[8])
        || (Registers.w127 && (Registers.pla[7] || Registers.pla[5]))
        );
    Registers.w280 = !(
        (Registers.w114 && ((Registers.w127 && Registers.pla[5]) || (Registers.w131 && Registers.w179)))
        || (Registers.w110 && Registers.w120 && !Registers.w178)
        || (Registers.w41 && Registers.w123 && Registers.w234)
        );

    if (bClk)
    {
        Registers.w391 = !Registers.w280;
    }

    Registers.w392 = !(Registers.w391 && !Registers.w162);

    Registers.w281 = !(
        (Registers.w110 && Registers.w123) || (Registers.w114 && Registers.w120)
        || (Registers.w109 && Registers.w131)
        );

    if (bClk)
    {
        Registers.l59 = Registers.w281;
    }

    Registers.w386 = !((Registers.w109 && Registers.w123 && Registers.w234)
        || (Registers.w41 && Registers.w127));
    
    if (bClk)
    {
        Registers.l58 = Registers.w386;
    }

    Registers.w435 = Registers.l58 && Registers.l59;
    Registers.w434 = !bClk && !Registers.w435;

    Registers.w282 = !(
        (Registers.w110 && ((Registers.w127 && (Registers.pla[5] || Registers.w255)) || (Registers.w131 && Registers.w177)))
        || (Registers.w68 && Registers.w131)
        || (Registers.w114 && Registers.w121 && Registers.w234)
        );

    if (bClk)
    {
        Registers.w431 = !Registers.w282;
    }

    Registers.w384 = !((Registers.w114 && Registers.w123 && Registers.w234)
        || (Registers.w109 && (Registers.w123 || Registers.w127) && Registers.w255));

    if (bClk)
    {
        Registers.w430 = !Registers.w384;
    }

    Registers.w429 = !(!Registers.l61 && (Registers.w430 || Registers.w431));
    Registers.w433 = !Registers.w429 && !bClk;
    Registers.w462 = !bClk && !Registers.w429;

    if (Registers.w434)
    {
        Registers.w442 = false;
    }
    if (Registers.w433)
    {
        Registers.w442 = true;
    }
    if (bClk)
    {
        Registers.l61 = Registers.w442;
    }

    Registers.w283 = !(
        !Registers.w274 ||
        (Registers.w68 && Registers.w131 && !Registers.w88)
        || (Registers.w41 && (Registers.w127 || Registers.w123) && Registers.w255)
        || (Registers.w109 && ((Registers.w131 && (Registers.w255 || Registers.w234))
            || (Registers.w123 && Registers.w234)))
        || (Registers.w114 && Registers.w120 && (Registers.w255 || Registers.w256))
        );

    if (bClk)
    {
        Registers.l56 = !Registers.w283;
    }

    Registers.w284 = !(Registers.w68 && Registers.w131 && Registers.w81);
    Registers.w285 = !(
        (Registers.w110 && Registers.w120 && Registers.pla[0])
        || (Registers.w114 && Registers.w127 && !Registers.w88)
        || (Registers.w41 && Registers.w121 && Registers.w86)
        );

    Registers.w286 = !(
        (Registers.w41 && Registers.w131 && Registers.w287)
        || (Registers.w110 && Registers.w123 && (Registers.w287 && Registers.w256))
        );

    Registers.w288 = !((Registers.w109 || Registers.w68) && (Registers.pla[21] || Registers.w77));

    if (bClk)
    {
        Registers.w370 = !Registers.w288;
    }

    Registers.w289 = !((Registers.w288 && Registers.w68 && Registers.w131) || Registers.w192);

    if (bClk)
    {
        Registers.w369 = !Registers.w289;
    }

    Registers.w290 = !(Registers.w216 || !Registers.w219 || !Registers.w226
        || (!Registers.w133 && (!Registers.w217 || Registers.w118)));

    Registers.w291 = !(!Registers.w226 || Registers.w216);

    if (bClk)
    {
        Registers.w292 = Registers.w291;
    }
    if (bClk)
    {
        Registers.w293 = Registers.w290;
    }

    Registers.w388 = !(Registers.pla[7] || Registers.w177);

    Registers.w387 = !(Registers.w109 && Registers.w127 && Registers.w388);

    if (bClk)
    {
        Registers.l60 = Registers.w387;
    }

    Registers.w437 = !bClk && !Registers.l60;

    Registers.w393 = !(!Registers.w277
        || (Registers.w114 && Registers.w127 && Registers.w255)
        || (Registers.w41 && Registers.w123 && Registers.w234)
        );

    if (bClk)
    {
        Registers.l66 = Registers.w393;
    }

    Registers.w458 = !bClk && !Registers.l66;

    Registers.w394 = !((Registers.w390 && !Registers.w166)
        || (Registers.w109 && Registers.w123 && Registers.w234)
        || (Registers.w41 && Registers.w127 && Registers.w255)
        );
    if (bClk)
    {
        Registers.l68 = Registers.w394;
    }

    Registers.w461 = !bClk && !Registers.l68;

    Registers.w395 = !(!Registers.w165 && Registers.w390);
    Registers.w459 = !bClk && !Registers.w395;

    Registers.w396 = !(Registers.w395 && Registers.w394 && (Registers.w390 || Registers.l53));

    if (bClk)
    {
        Registers.l67 = Registers.w396;
    }

    Registers.w460 = !bClk && !Registers.l67;

    Registers.w83 = !(Registers.w77 || !Registers.w86);
    Registers.w398 = !(Registers.w114 && Registers.w123 && Registers.w83);
    
    if (bClk)
    {
        Registers.l76 = Registers.w398;
    }

    if (bClk)
    {
        Registers.w400 = (
            (Registers.w41 && Registers.w127 && Registers.w255)
            || (Registers.w114 && Registers.w123 && Registers.pla[38])
            );
    }

    Registers.w399 = !((Registers.w390 && !Registers.pla[36] && Registers.w255) || Registers.w400);
    Registers.w490 = !bClk && !Registers.w399;
    Registers.w479 = Registers.l76 && Registers.w399;
    Registers.w491 = !bClk && !Registers.w479;

    Registers.w401 = !(
        (((Registers.w147 & 8) == 0 && Registers.w109) || Registers.w114) &&
        Registers.w127 && Registers.pla[38]
        );

    if (bClk)
    {
        Registers.l77 = Registers.w401;
    }

    Registers.w480 = !bClk && !Registers.l77;
}

void FCPU_Z80_NMOS::InterruptLogic2(bool bClk)
{
    if (bClk)
    {
        Registers.l26 = !(Registers.w131 && Registers.pla[1] && Registers.w110);
    }

    Registers.w76 = !(Registers.pla[52] && Registers.w131 && Registers.w114);

    if (bClk)
    {
        Registers.l27 = Registers.w76;
    }
    if (bClk)
    {
        Registers.l3 = !Registers.w73 || Registers.pla[1];
    }

    Registers.w13 = !((Registers.w16 && !Registers.w10) || Registers.w18 || Registers.w19 || Registers.halt);
    Registers.w14 = !(Registers.w13 || (Registers.w16 && Registers.w10));

    if (bClk)
    {
        Registers.l28 = !(Registers.pla[2] && Registers.w131 && Registers.w110);
    }

    Registers.w81 = Registers.w80 && (Registers.w89 && Registers.w78 && Registers.w18);

    if (bClk)
    {
        Registers.l37 = Registers.w133;
        Registers.l38 = !Registers.w110 && !Registers.w55;
    }
    else
    {
        if (!Registers.l37)
        {
            Registers.w144 = true;
        }
        if (!Registers.l38)
        {
            Registers.w144 = false;
        }
    }

    Registers.w193 = (Registers.w144 && Registers.w14) || Registers.w205;
}

void FCPU_Z80_NMOS::RegistersLogic(bool bClk)
{
    Registers.w315 = ((Registers.w147 & 4) == 0 && Registers.w183) || (!Registers.w183 && (Registers.w147 & 32) == 0);
    Registers.w317 = ((Registers.w147 & 2) == 0 && Registers.w183) || (!Registers.w183 && (Registers.w147 & 16) == 0);
    Registers.w318 = ((Registers.w147 & 1) == 0 && Registers.w183) || (!Registers.w183 && (Registers.w147 & 8) == 0);

    Registers.w406 = Registers.pla[50] || (Registers.w147 & 32) == 0;

    Registers.w405 = !((Registers.w147 & 16) == 0 || !Registers.w406);
    Registers.w407 = !((Registers.w147 & 16) == 0 || Registers.w406);
    Registers.w408 = !((Registers.w147 & 16) != 0 || Registers.w406);
    Registers.w409 = !((Registers.w147 & 16) != 0 || !Registers.w406);

    Registers.w296 = !(Registers.w299 || Registers.w153);
    Registers.w295 = !(Registers.w133 ||
        (Registers.w223 && (!Registers.w118 || Registers.w296)));

    Registers.w297 = !(!Registers.w226 || Registers.w295 || Registers.w294);

    if (bClk)
    {
        Registers.w298 = Registers.w297;
    }

    Registers.w300 = !(!Registers.w226 || Registers.w295);

    if (bClk)
    {
        Registers.w301 = !Registers.w300;
    }
    if (bClk)
    {
        Registers.w305 = Registers.w228 && Registers.w227;
    }
    else
    {
        if (!Registers.w293 && Registers.w305)
        {
            Registers.w321 = false;
        }
        else if (!Registers.w305)
        {
            Registers.w321 = true;
        }
    }

    Registers.w306 = !(Registers.w228 && Registers.w227 && Registers.w229);

    if (bClk)
    {
        Registers.w307 = Registers.w306 || Registers.w55;
    }

    Registers.w309 = !(Registers.w231 && Registers.w230);
    Registers.w313 = !(!Registers.w246 || !Registers.w243 || !Registers.w242 || !Registers.w274 || !Registers.w241 || !Registers.w239 || !Registers.w238 || Registers.w309);
    Registers.w308 = !(!Registers.w294 && Registers.w313 && Registers.w344);
    Registers.w310 = !(Registers.w233 && Registers.w232);
    Registers.w312 = !(Registers.w100 || !Registers.w169);
    Registers.w311 = !(Registers.w238 && (Registers.w343 || !Registers.w312));

    if (bClk)
    {
        Registers.l44 = Registers.w308;
    }

    Registers.w314 = Registers.w307 || Registers.l44;
    Registers.w316 = !(Registers.w315 || Registers.w317);

    if (bClk)
    {
        Registers.w319 = !((Registers.w316 && Registers.w310) || Registers.w309);
    }

    Registers.w227 = !(Registers.w131 && Registers.w114);

    if (bClk)
    {
        Registers.l42 = Registers.w227;
    }

    Registers.w303 = !Registers.l42;

    if (bClk)
    {
        Registers.l45 = Registers.w315;
    }
    else if (Registers.w304)
    {
        Registers.w320 = Registers.l45;
    }
    if (bClk)
    {
        Registers.w322 = !Registers.w113;
    }

    Registers.w326 = !(Registers.w303 && Registers.pla[96]);
    Registers.w302 = !(Registers.w303 && Registers.pla[97]);
    Registers.w325 = !(Registers.w302 && Registers.w326);
    Registers.w324 = !bClk && !Registers.w325;

    if (Registers.w324)
    {
        Registers.l46 = Registers.w327;
    }
    if (Registers.w328)
    {
        Registers.w327 = !Registers.l46;
    }

    Registers.w323 = !((bClk && !Registers.w113) || Registers.w322);

    if (Registers.w324)
    {
        Registers.l47 = Registers.w330;
    }

    if (Registers.w329)
    {
        Registers.w330 = !Registers.l47;
    }
    if (Registers.w324)
    {
        Registers.l48 = Registers.w332;
    }
    if (Registers.w331)
    {
        Registers.w332 = !Registers.l48;
    }

    Registers.w333 = !((Registers.w327 && Registers.w332) || (!Registers.w327 && Registers.w330));
    Registers.w334 = !bClk && !Registers.w293;
    Registers.w335 = !bClk && !Registers.w292;
    Registers.w336 = !bClk && !Registers.w298;
    Registers.w337 = !bClk && Registers.w307;
    Registers.w338 = Registers.w305 && !Registers.w301;
    Registers.w339 = bClk;

    if (bClk)
    {
        Registers.w341 = !Registers.w311;
    }

    Registers.w340 = !(!Registers.w341 && !Registers.w320);
    Registers.w342 = !(!Registers.w341 && Registers.w320);

    Registers.w344 = !(Registers.w310 || !Registers.w244 || !Registers.w250 || !Registers.w248);

    Registers.w343 = !(!Registers.w241 || !Registers.w239 || (!Registers.w315 && Registers.w317 && (Registers.w310 || !Registers.w344)));

    if (bClk)
    {
        Registers.w345 = Registers.w343 || Registers.w312;
    }
    if (bClk)
    {
        Registers.w346 = Registers.w242 && (!Registers.w315 || Registers.w317 || Registers.w344);
    }
    if (bClk)
    {
        Registers.w347 = (Registers.w344 || !Registers.w315 || !Registers.w317) && Registers.w243;
    }

    Registers.w349 = !(Registers.w333 ? Registers.w346 : Registers.w345);

    Registers.w348 = !(!Registers.w327 && Registers.w349);
    Registers.w350 = !(Registers.w327 && Registers.w349);

    Registers.w351 = !(Registers.w333 ? Registers.w345 : Registers.w346);

    Registers.w352 = !(!Registers.w327 && Registers.w351);
    Registers.w353 = !(Registers.w327 && Registers.w351);

    Registers.w354 = !(!Registers.w327 && !Registers.w347);
    Registers.w355 = !(Registers.w327 && !Registers.w347);

    Registers.w356 = !(Registers.w315 || Registers.w317);
    Registers.w357 = !((Registers.w248 && Registers.w250) || Registers.w318);

    if (bClk)
    {
        Registers.l81 = !(!Registers.w274 || !Registers.w246 || (Registers.w356 && (Registers.w357 || !Registers.w244)));
    }

    Registers.w358 = !Registers.l81;

    if (bClk)
    {
        Registers.w359 = !Registers.w259;
    }

    Registers.w360 = !bClk && !Registers.w359;
    Registers.w362 = !bClk && Registers.w359;

    if (Registers.w360)
    {
        Registers.l49 = Registers.w361;
    }
    if (Registers.w362)
    {
        Registers.w361 = !Registers.l49;
    }

    Registers.w363 = !(Registers.w361 && Registers.w358);
    Registers.w364 = !(!Registers.w361 && Registers.w358);
    Registers.w365 = !(Registers.w248 && (Registers.w249 || Registers.w317));
    Registers.w366 = !(Registers.w250 && (Registers.w249 || !Registers.w317));
    Registers.w368 = !(Registers.w315 || Registers.w317);
    Registers.w367 = !(Registers.w318 || Registers.w368);
    Registers.w410 = !(!Registers.w257 || !Registers.w258 || (Registers.w366 && (Registers.w318 || Registers.w368)));
    Registers.w411 = !(!Registers.w251 || (Registers.w366 && Registers.w367) || (!Registers.w274 && !Registers.w380));
    Registers.w412 = !(Registers.w260 && Registers.w262);
    Registers.w413 = !(Registers.w318 || Registers.w368);
    Registers.w414 = !(!Registers.w264 || !Registers.w263 || !Registers.w274
        || (Registers.w365 && (Registers.w318 || Registers.w368)));
    Registers.w415 = !(Registers.w412 || (Registers.w413 && Registers.w365) || (!Registers.w274 && Registers.w380));

    if (bClk)
    {
        Registers.w417 = !Registers.w415;
    }
    if (bClk)
    {
        Registers.w418 = !Registers.w414;
    }
    if (bClk)
    {
        Registers.w416 = Registers.w410;
    }

    Registers.w517 = !bClk && !Registers.w416;

    if (bClk)
    {
        Registers.l79 = Registers.w411;
    }

    Registers.w516 = !bClk && !Registers.l79;
    Registers.w518 = !Registers.w417;
    Registers.w519 = !Registers.w418;
}

void FCPU_Z80_NMOS::AluControlLogic(bool bClk)
{
    if (bClk)
    {
        Registers.w372 = !Registers.w286;
    }
    if (bClk)
    {
        Registers.w373 = !Registers.w285 || !Registers.w284;
    }
    if (Registers.w370)
    {
        Registers.bu2 = 0xFF;
        Registers.w484 = 0x99;

        if (!Registers.w444)
        {
            Registers.w484 |= 6;
        }
        if (!Registers.w443)
        {
            Registers.w484 |= 0x60;
        }
    }

    Registers.w371 = !(!Registers.w286 || !Registers.w284 || (Registers.w271 && !Registers.w268));

    if (bClk)
    {
        Registers.l54 = Registers.w371;
    }

    Registers.w403 = !(!Registers.w283 || !Registers.w269 || !Registers.w268);

    if (bClk)
    {
        Registers.w404 = Registers.w403 || !Registers.w371;
    }

    Registers.w493 = !Registers.w404;

    if (bClk)
    {
        Registers.l73 = !Registers.w473;
    }

    Registers.w423 = Registers.l73;
    Registers.w422 = !(Registers.w408 || (Registers.w405 && Registers.w423));
    Registers.w376 = !(
        (Registers.w41 && Registers.w123 && Registers.w234)
        || (Registers.w114 && Registers.w127 && Registers.w255)
        );
    Registers.w375 = !(Registers.w376 && Registers.w275);
    
    if (bClk)
    {
        Registers.w377 = Registers.w375;
    }

    Registers.w374 = !(!Registers.w285 || !Registers.w284 || !Registers.w266 || !Registers.w267 || Registers.w375);
    
    if (bClk)
    {
        Registers.l55 = Registers.w374;
    }

    Registers.w402 = !Registers.l54 || Registers.l55;

    if (!bClk)
    {
        if (Registers.w461)
        {
            Registers.w476 = !Registers.w423;
        }
        if (Registers.w460)
        {
            Registers.w476 = !true;
        }
        if (Registers.w459)
        {
            Registers.w476 = !false;
        }
        if (Registers.w462)
        {
            Registers.w476 = Registers.l75;
        }
    }

    if (Registers.w381)
    {
        Registers.bu2 |= 0xD7;
        Registers.w484 &= ~0xD7;

        if (!Registers.w481)
        {
            Registers.w484 |= 2; // N
        }
        if (!Registers.w473)
        {
            Registers.w484 |= 1; // C
        }
        if (!Registers.w476)
        {
            Registers.w484 |= 16; // H
        }
        if (!Registers.w450)
        {
            Registers.w484 |= 128; // S
        }
        if (!Registers.w441)
        {
            Registers.w484 |= 4; // P/V
        }
        if (!Registers.w486)
        {
            Registers.w484 |= 64; // Z
        }
    }

    Registers.w385 = !(Registers.w114 || Registers.w109);

    if (bClk)
    {
        Registers.l57 = Registers.w385;
    }

    Registers.w432 = !bClk && Registers.l57;
    Registers.w426 = !(Registers.w390 && !Registers.w154);
    Registers.w427 = !bClk && !Registers.w426;
    Registers.w428 = Registers.l56 && Registers.w426 && !bClk;
    Registers.w446 = !Registers.w442 && !Registers.w433;
}

void FCPU_Z80_NMOS::CalcAlu()
{
    bool  o1, o2, t, t2, c;

    if (Registers.alu_calc)
    {
        return;
    }

    {
        uint8_t t;
        if (Registers.w481)
        {
            t = Registers.w511 ^ 0xFF;
        }
        else
        {
            t = Registers.w511;
        }

        if (Registers.w446)
        {
            Registers.w512 = t & 0x0F;
        }
        else
        {
            Registers.w512 = (t >> 4) & 0x0F;
        }
    }

    Registers.w504 = 0;
    c = !(Registers.w467 ^ Registers.w476);

    o1 = (Registers.w512 & 1) != 0;
    o2 = (Registers.w500 & 1) != 0;
    t = !((c && (o1 || o2)) || (o1 && o2) || Registers.w455);
    t2 = ((o1 || o2 || c) && (t || Registers.w454)) || (o1 && o2 && c);
    c = !t && !Registers.w456;
    Registers.w504 |= t2 << 0;

    o1 = (Registers.w512 & 2) != 0;
    o2 = (Registers.w500 & 2) != 0;
    t = !((c && (o1 || o2)) || (o1 && o2) || Registers.w455);
    t2 = ((o1 || o2 || c) && (t || Registers.w454)) || (o1 && o2 && c);
    c = !t && !Registers.w456;
    Registers.w504 |= t2 << 1;

    o1 = (Registers.w512 & 4) != 0;
    o2 = (Registers.w500 & 4) != 0;
    t = !((c && (o1 || o2)) || (o1 && o2) || Registers.w455);
    t2 = ((o1 || o2 || c) && (t || Registers.w454)) || (o1 && o2 && c);
    c = !t && !Registers.w456;
    Registers.w504 |= t2 << 2;

    Registers.w508 = c;

    o1 = (Registers.w512 & 8) != 0;
    o2 = (Registers.w500 & 8) != 0;
    t = !((c && (o1 || o2)) || (o1 && o2) || Registers.w455);
    t2 = ((o1 || o2 || c) && (t || Registers.w454)) || (o1 && o2 && c);
    c = !t && !Registers.w456;
    Registers.w504 |= t2 << 3;

    Registers.w507 = c;

    Registers.alu_calc = true;
}

void FCPU_Z80_NMOS::AluLogic(bool bClk)
{
    if (bClk)
    {
        Registers.l63 = Registers.w180;
    }

    Registers.w454 = !Registers.l63 && !Registers.w115;

    if (bClk)
    {
        Registers.l64 = Registers.w181;
    }

    Registers.w455 = !Registers.l64 && !Registers.w115;

    if (bClk)
    {
        Registers.l65 = Registers.w168;
    }

    Registers.w456 = !Registers.l65 && !Registers.w115;

    if (bClk)
    {
        Registers.w453 = Registers.pla[0x0F];
    }
    if (Registers.w378)
    {
        Registers.w496 = Registers.w511;
    }
    if (!Registers.w402)
    {
        Registers.bu3 = 0xFF;
        Registers.w513 = Registers.w496 ^ 0xFF;
    }
}

void FCPU_Z80_NMOS::RegistersLogic2(bool bClk)
{
    Registers.pull1[0] = 0;
    Registers.pull1[1] = 0;
    Registers.pull2[0] = 0;
    Registers.pull2[1] = 0;
    Registers.ix = -1;
    if (!bClk)
    {
        if (!Registers.w364)            // af
        {
            Registers.ix = 0;
        }
        else if (!Registers.w363)       // af'
        {
            Registers.ix = 1;
        }
        else if (!Registers.w355)       // bc'
        {
            Registers.ix = 2;
        }
        else if (!Registers.w354)       // bc
        {
            Registers.ix = 3;
        }
        else if (!Registers.w353)       // hl
        {
            Registers.ix = 4;
        }
        else if (!Registers.w352)       // hl'
        {
            Registers.ix = 5;
        }
        else if (!Registers.w350)       // de
        {
            Registers.ix = 6;
        }
        else if (!Registers.w348)       // de'
        {
            Registers.ix = 7;
        }
        else if (!Registers.w342)       // ix
        {
            Registers.ix = 8;
        }
        else if (!Registers.w340)       // iy
        {
            Registers.ix = 9;
        }
        else if (!Registers.w319)       // sp
        {
            Registers.ix = 10;
        }
        else if (!Registers.w314)       // wz ?
        {
            Registers.ix = 11;
        }
        if (Registers.ix >= 0)
        {
            Registers.pull1[0] |= Registers.regs_[Registers.ix][1];
            Registers.pull1[1] |= Registers.regs_[Registers.ix][0];
        }
#if 0
        if (!bClk && Registers.ix >= 0)
        {
            Registers.w514 = Registers.regs_[Registers.ix][0];
            Registers.w515 = Registers.regs_[Registers.ix][1];

            if (Registers.w338)
            {
                Registers.w520 = Registers.w514;
                Registers.w521 = Registers.w515;
            }
        }
#endif
    }
    if (!Registers.w215)
    {
        Registers.w528 = Registers.w527 ^ 0xFFFF;
    }
    else
    {
        Registers.w528 = 0x0000;
    }

    Registers.w529 = Registers.w528 ^ 0xFFFF;

    if (Registers.w335)
    {
        Registers.pull2[0] |= Registers.w529;
        Registers.pull2[1] |= Registers.w528;
    }
    if (Registers.w336)
    {
        Registers.pull2[0] |= Registers.regs2_[0][1];
        Registers.pull2[1] |= Registers.regs2_[0][0];
    }
    if (Registers.w337)
    {
        Registers.pull2[0] |= Registers.regs2_[1][1];
        Registers.pull2[1] |= Registers.regs2_[1][0];
    }
    if (Registers.w338)
    {
        int pull = Registers.pull1[0] | Registers.pull2[0];
        Registers.pull1[0] = pull;
        Registers.pull2[0] = pull;
        pull = Registers.pull1[1] | Registers.pull2[1];
        Registers.pull1[1] = pull;
        Registers.pull2[1] = pull;
    }

    //Registers.w514 &= ~pull1[0];
    //Registers.w515 &= ~pull1[1];
    //
    //Registers.w520 &= ~pull2[0];
    //Registers.w521 &= ~pull2[1];

    if (bClk && Registers.w210)
    {
        Registers.w524 = (Registers.w522 & 0xFFFF) != 1;
    }

    Registers.w438 = !Registers.w524;

    if (bClk)
    {
        if (Registers.w339)
        {
            Registers.w514 = 0xFFFF;
            Registers.w515 = 0xFFFF;
            if (Registers.w338)
            {
                Registers.w520 = 0xFFFF;
                Registers.w521 = 0xFFFF;
            }
            {
                int32_t i;
                bool c[16], o[16], o2[15];
                Registers.w523 = 0;
                Registers.w525 = Registers.w522 & 0x7FFF;
                if (!Registers.w210)
                {
                    Registers.w525 ^= 0x7FFF;
                }
                for (i = 0; i < 16; i++)
                {
                    o[i] = (Registers.w522 >> i) & 1;
                }
                for (i = 0; i < 15; i++)
                {
                    o2[i] = (Registers.w525 >> i) & 1;
                }
                c[0] = !Registers.w193;
                c[1] = !Registers.w193 && !o2[0];
                c[2] = !Registers.w193 && !o2[0] && !o2[1];
                c[3] = !o2[2] && c[2];
                c[4] = !o2[3] && !o2[2] && c[2];
                c[5] = !o2[4] && c[4];
                c[6] = !o2[5] && !o2[4] && c[4];
                c[7] = !o2[6] && !o2[5] && !o2[4] && !o2[3] && !o2[2] && !o2[1] && !o2[0] && !Registers.w193 && !Registers.w321;
                c[8] = !o2[7] && c[7];
                c[9] = !o2[8] && !o2[7] && c[7];
                c[10] = !o2[9] && c[9];
                c[11] = !o2[10] && !o2[9] && c[9];
                c[12] = !o2[11] && !o2[10] && !o2[9] && !o2[8] && !o2[7] && c[7];
                c[13] = !o2[12] && c[12];
                c[14] = !o2[13] && !o2[12] && c[12];
                c[15] = !o2[14] && !o2[13] && !o2[12] && c[12];

                for (i = 0; i < 16; i++)
                {
                    Registers.w523 |= (c[i] ^ o[i] ^ 1) << i;
                }
            }
            Registers.w527 = Registers.w523;
        }
    }

    Registers.w194 = !Registers.w202 && !Registers.w203;
    if (bClk && Registers.w194)
    {
        Registers.w526 = Registers.w522 ^ 0xFFFF;
    }

    //Registers.o_addr_high = true;
    if (!Registers.w323)
    {
        SB->SetDataOnAddressBus(Registers.w526 ^ 0xFFFF);
        //Registers.o_addr_high = false;
    }

    Registers.w514 = Registers.pull1[0] ^ 0xFFFF;
    Registers.w515 = Registers.pull1[1] ^ 0xFFFF;
    Registers.w520 = Registers.pull2[0] ^ 0xFFFF;
    Registers.w521 = Registers.pull2[1] ^ 0xFFFF;

    if (!bClk)
    {
        if (Registers.w334)
        {
            Registers.w522 = Registers.w520;
        }
    }
    if (!bClk)
    {
        if (!Registers.w518)
        {
            Registers.bu2 = 0xFF;
            Registers.w484 = (Registers.w515) & 0xFF;
        }
        if (!Registers.w519)
        {
            Registers.bu3 = 0xFF;
            Registers.w513 = (Registers.w515 >> 8) & 0xFF;
        }
    }
}

void FCPU_Z80_NMOS::BusLogic2(bool bClk)
{
    {
        uint8_t val = 0;
        uint8_t val2 = 0;
        uint8_t umask = 0;
        if (Registers.w369 && Registers.w419)
        {
            umask |= Registers.bu1 | Registers.bu2 | Registers.bu3;
            val |= (Registers.w146 & Registers.bu1);
            val |= (Registers.w484 & Registers.bu2);
            val |= (Registers.w513 & Registers.bu3);
            val2 = 0xFF;
            val2 &= Registers.w146;
            val2 &= Registers.w484;
            val2 &= Registers.w513;
            val2 &= ~umask;
            Registers.w146 = val | val2;
            Registers.w484 = val | val2;
            Registers.w513 = val | val2;
        }
        else if (Registers.w369) // bus1, bus2
        {
            umask |= Registers.bu1 | Registers.bu2;
            val |= (Registers.w146 & Registers.bu1);
            val |= (Registers.w484 & Registers.bu2);
            val2 = 0xFF;
            val2 &= Registers.w146;
            val2 &= Registers.w484;
            val2 &= ~umask;
            Registers.w146 = val | val2;
            Registers.w484 = val | val2;
        }
        else if (Registers.w419) // bus2, bus3
        {
            umask |= Registers.bu2 | Registers.bu3;
            val |= (Registers.w484 & Registers.bu2);
            val |= (Registers.w513 & Registers.bu3);
            val2 = 0xFF;
            val2 &= Registers.w484;
            val2 &= Registers.w513;
            val2 &= ~umask;
            Registers.w484 = val | val2;
            Registers.w513 = val | val2;
        }
    }
    if (Registers.w2)
    {
        Registers.w145 = SB->GetDataOnDataBus() ^ 0xFF;
    }
    else if (Registers.w42)
    {
        Registers.w145 = Registers.w146;
    }

    //Registers.ext_data_o_high = true;

    if (!Registers.w44)
    {
        SB->SetDataOnDataBus(Registers.w145 ^ 0xFF);
        //Registers.ext_data_o_high = false;
    }
}

void FCPU_Z80_NMOS::AluLogic2(bool bClk)
{
    if (bClk)
    {
        Registers.w425 = true;
    }
    else
    {
        if (Registers.w407)
        {
            Registers.w425 = true;
        }
        else if (Registers.w424 && Registers.w405)
        {
            Registers.w425 = Registers.w423;
        }
        else if (Registers.w424 && Registers.w408)
        {
            Registers.w425 = (Registers.w484 & 128) != 0;
        }
        else if (Registers.w424 && Registers.w409)
        {
            Registers.w425 = (Registers.w484 & 1) != 0;
        }
    }

    Registers.w495 = Registers.w409 && (Registers.w147 & 8) == 0 && Registers.w470;

    if (Registers.w493)
    {
        Registers.w496 = Registers.w513;
    }
    if (Registers.w471)
    {
        Registers.w496 = (Registers.w513 << 1) & 0xff;
        if (!Registers.w495)
        {
            Registers.w496 |= !Registers.w422;
        }
    }
    if (Registers.w472)
    {
        Registers.w496 = (Registers.w513 >> 1) & 0x7f;
        Registers.w496 |= uint8_t(Registers.w425) << 7;
    }
    Registers.w497 = (1 << (((Registers.w146 >> 3) & 7) ^ 7)) ^ 0xFF;

    if (Registers.w372)
    {
        Registers.w496 = Registers.w497;
    }

    if (Registers.w373)
    {
        Registers.w496 = Registers.w498;
    }

    if (Registers.w495)
    {
        Registers.w496 &= 0xFE;
        Registers.w496 |= (Registers.w484 >> 7) & 1;
    }

    if (!bClk)
    {
        if (Registers.w436)
        {
            Registers.w445 = 0;
        }
        else if (Registers.w382)
        {
            Registers.w445 = (Registers.w484 & 64) != 0;
        }
        else if (Registers.w440)
        {
            CalcAlu();
            Registers.w445 = (Registers.w487 || (Registers.w503 & 0x0F) != 0 || (Registers.w504 & 0x0F) != 0);
        }
    }
    Registers.w486 = !Registers.w445;
    Registers.w487 = !Registers.w486;

    Registers.w439 = !bClk && !Registers.w162 && !Registers.w429;

    if (bClk)
    {
        Registers.w452 = !Registers.w158;
    }
    if (bClk)
    {
        CalcAlu();
        Registers.w505 = !(((Registers.w485 ^ ((Registers.w504 & 1) != 0)) ^ ((Registers.w504 & 2) != 0)) ^ ((Registers.w504 & 4) != 0));
        Registers.l62 = Registers.w505;
    }
    if (bClk)
    {
        Registers.w449 = !Registers.l78;
    }
    if (Registers.w432)
    {
        Registers.l83 = (Registers.w484 & 1) != 0;
    }
    if (Registers.w432)
    {
        Registers.l84 = (Registers.w484 & 16) != 0;
    }
    Registers.w466 = !bClk && Registers.w390 && !Registers.pla[21];
    Registers.w457 = (Registers.pla[30] && (Registers.w147 & 8) == 0) || !Registers.w160;

    if (!bClk)
    {
        if (Registers.w382)
        {
            Registers.w464 = (Registers.w484 & 2) == 0;
        }
        if (Registers.w465)
        {
            Registers.w464 = (Registers.w484 & 128) == 0;
        }
        if (Registers.w466)
        {
            Registers.w464 = Registers.w457;
        }
    }
    Registers.w397 = !((Registers.w41 || Registers.w109 || Registers.w68) && Registers.w127);
    if (bClk)
    {
        Registers.l72 = Registers.w397;
    }
    Registers.w467 = !(Registers.w464 && !Registers.w115);
    Registers.w468 = !(Registers.w464 && !(Registers.w115 && Registers.l72));
    Registers.w481 = !Registers.w468;
    if (bClk)
    {
        Registers.w470 = !Registers.w268 && Registers.w271;
    }

    Registers.w469 = !bClk && Registers.w470;

    Registers.w471 = Registers.w470 && (Registers.w147 & 8) == 0;
    Registers.w472 = Registers.w470 && (Registers.w147 & 8) != 0;
    Registers.w424 = !bClk && Registers.w472;

    Registers.w475 = !(Registers.w443 && Registers.w370);
    Registers.w474 = !bClk && !Registers.w475;

    Registers.w477 = !(Registers.w467 ^ Registers.w507);

    if (Registers.w446 && bClk)
    {
        Registers.l75 = Registers.w477;
    }

    if (!bClk)
    {
        if (Registers.w469)
        {
            if (Registers.w472)
            {
                Registers.w473 = (Registers.w484 & 1) == 0;
            }
            if (Registers.w471)
            {
                Registers.w473 = (Registers.w484 & 128) == 0;
            }
        }
        if (Registers.w382)
        {
            Registers.w473 = (Registers.w484 & 1) == 0;
        }
        if (Registers.w474)
        {
            Registers.w473 = !0;
        }
        if (Registers.w458)
        {
            Registers.w473 = Registers.w477;
        }
        if (Registers.w463)
        {
            Registers.w473 = !Registers.w476;
        }
    }
    if (Registers.w432)
    {
        Registers.w499 = (Registers.w496 >> 4) ^ 0x0F;
    }
    if (!bClk)
    {
        if (Registers.w428)
        {
            Registers.w498 = (Registers.w496 & 0xFF) ^ 0xFF;
        }
        if (Registers.w427)
        {
            Registers.w498 = 0x00;
        }
        if (Registers.w480)
        {
            Registers.w498 &= 0xF0;
            Registers.w498 |= (Registers.w499 & 0x0F) ^ 0x0F;
        }
    }
    Registers.w502 = !((Registers.w498 & 8) != 0 && ((Registers.w498 & 4) != 0 || (Registers.w498 & 2) != 0));
    Registers.w501 = !((Registers.w498 & 128) != 0 && ((Registers.w498 & 64) != 0 || (Registers.w498 & 32) != 0
        || ((Registers.w498 & 16) != 0 && !Registers.w502)));
    Registers.w443 = !(Registers.pla[21] && Registers.l83 && Registers.w501);
    Registers.w444 = !(Registers.pla[21] && Registers.l84 && Registers.w502);

    if (Registers.w446)
    {
        Registers.w500 = Registers.w498 & 0x0F;
    }
    else
    {
        Registers.w500 = (Registers.w498 >> 4) & 0x0F;
    }
    if (Registers.w432)
    {
        Registers.w510 = ((Registers.w500 & 0x0F) | ((Registers.w496 & 0x0F) << 4)) ^ 0xFF;
    }
    if (!bClk)
    {
        if (Registers.w480)
        {
            Registers.w511 = Registers.w510 ^ 0xFF;
        }
        if (Registers.w492)
        {
            Registers.w511 = Registers.w496 ^ 0xFF;
        }
        if (Registers.w490 && Registers.w491)
        {
            Registers.w511 = 0x00;
        }
        else if (Registers.w491)
        {
            Registers.w511 &= 0x38;
        }
        else if (Registers.w490)
        {
            Registers.w511 &= 0x7F;
        }
    }

    if (Registers.w446)
    {
        CalcAlu();
        Registers.w503 = Registers.w504;
    }

    if (Registers.w377)
    {
        CalcAlu();
        Registers.w496 = Registers.w503 | (Registers.w504 << 4);
    }

    if (!bClk)
    {
        if (Registers.w382)
        {
            Registers.w441 = (Registers.w484 & 4) == 0;
        }
        else if (Registers.w437)
        {
            Registers.w441 = !Registers.w438;
        }
        else if (Registers.w436)
        {
            Registers.w441 = !false;
        }
        else if (Registers.w439)
        {
            Registers.w441 = !Registers.w449;
        }
        else if (Registers.w440)
        {
            if (Registers.w452)
            {
                Registers.w441 = (Registers.w508 ^ Registers.w507) && !Registers.w453;
            }
            else
            {
                CalcAlu();
                Registers.w506 = ((Registers.w504 & 8) != 0) ^ ((Registers.w503 & 8) != 0);
                Registers.w441 = Registers.w506 ^ Registers.l62;
            }
        }
    }

    Registers.w485 = !Registers.w441;

    if (Registers.w446)
    {
        CalcAlu();
        Registers.w505 = !(((Registers.w485 ^ ((Registers.w504 & 1) != 0)) ^ ((Registers.w504 & 2) != 0)) ^ ((Registers.w504 & 4) != 0));
        Registers.l78 = Registers.w505;
    }
    if (!bClk)
    {
        if (Registers.w382)
        {
            Registers.w450 = (Registers.w484 & 128) == 0;
        }
        else if (Registers.w440)
        {
            CalcAlu();
            Registers.w450 = (Registers.w504 & 8) != 0;
        }
    }
}

void FCPU_Z80_NMOS::RegistersLogic3(bool bClk)
{
    if (!bClk)
    {
        if (Registers.w516)
        {
            Registers.pull1[0] |= Registers.w484 & 0xFF;
            Registers.pull1[1] |= (Registers.w484 & 0xFF) ^ 0xFF;
        }
        if (Registers.w517)
        {
            Registers.pull1[0] |= (Registers.w513 & 0xFF) << 8;
            Registers.pull1[1] |= ((Registers.w513 ^ 0xFF) & 0xFF) << 8;
        }
        if (!bClk && Registers.ix >= 0)
        {
            if (Registers.ix == 1)
            {
                int a = 10;
            }
            uint16_t p1 = Registers.pull1[0] | Registers.regs_[Registers.ix][1];
            Registers.regs_[Registers.ix][0] = p1 ^ 0xFFFF;
            uint16_t p2 = Registers.pull1[1] | Registers.regs_[Registers.ix][0];
            Registers.regs_[Registers.ix][1] = p2 ^ 0xFFFF;
        }
    }
    if (Registers.w338)
    {
        uint16_t pull = Registers.pull1[0] | Registers.pull2[0];
        Registers.pull1[0] = pull;
        Registers.pull2[0] = pull;
        pull = Registers.pull1[1] | Registers.pull2[1];
        Registers.pull1[1] = pull;
        Registers.pull2[1] = pull;
    }
    if (Registers.w336)
    {
        uint16_t p1 = Registers.pull2[0] | Registers.regs2_[0][1];
        Registers.regs2_[0][0] = p1 ^ 0xFFFF;
        uint16_t p2 = Registers.pull2[1] | Registers.regs2_[0][0];
        Registers.regs2_[0][1] = p2 ^ 0xFFFF;
    }
    if (Registers.w337)
    {
        uint16_t p1 = Registers.pull2[0] | Registers.regs2_[1][1];
        Registers.regs2_[1][0] = p1 ^ 0xFFFF;
        uint16_t p2 = Registers.pull2[1] | Registers.regs2_[1][0];
        Registers.regs2_[1][1] = p2 ^ 0xFFFF;
    }

    Registers.w514 = Registers.pull1[0] ^ 0xFFFF;
    Registers.w515 = Registers.pull1[1] ^ 0xFFFF;
    Registers.w520 = Registers.pull2[0] ^ 0xFFFF;
    Registers.w521 = Registers.pull2[1] ^ 0xFFFF;

    if (!bClk)
    {
        if (Registers.w334)
        {
            Registers.w522 = Registers.w520;
        }
    }
}

void FCPU_Z80_NMOS::ConditionLogic(bool bClk)
{
    Registers.w494 = !(((Registers.w484 & 64) != 0 && Registers.w409)
        || ((Registers.w484 & 4) != 0 && Registers.w408)
        || ((Registers.w484 & 128) != 0 && Registers.w407)
        || ((Registers.w484 & 1) != 0 && Registers.w405));

    Registers.w421 = !bClk && !Registers.w419;

    if (!bClk && Registers.w421)
    {
        Registers.w420 = Registers.w494;
    }

    Registers.w448 = !(Registers.w420 ^ Registers.w318);
    Registers.w383 = !(Registers.w279 || !Registers.w486);
    Registers.w299 = (!Registers.w220 && Registers.w438) || Registers.w383 || (Registers.w265 && Registers.w448);

    // end of instruction
    if (bClk)
    {
        Registers.l4 = !(Registers.w55 || !Registers.w97 || !Registers.w118 || Registers.w133);
    }
}

void FCPU_Z80_NMOS::Clock()
{
    Registers.bu1 = 0;
    Registers.bu2 = 0;
    Registers.bu3 = 0;
    Registers.alu_calc = 0;

    const bool bClk = !!(Registers.CC & 0x01);
    ClkLatches(bClk);
    ResetLogic(bClk);
    StateLogic(bClk);
    InterruptLogic(bClk);
    IOLogic(bClk);
    BusLogic(bClk);
    OpcodeFetch(bClk);
    HaltLogic(bClk);
    InterruptFlipFlops(bClk);
    OpcodeDecode(bClk);
    InterruptLogic2(bClk);
    RegistersLogic(bClk);
    AluControlLogic(bClk);
    AluLogic(bClk);
    RegistersLogic2(bClk);
    BusLogic2(bClk);
    AluLogic2(bClk);
    RegistersLogic3(bClk);
    ConditionLogic(bClk);
}
