#pragma once

// This resource is taken as a basis
// https://github.com/nukeykt/Nuked-MD/blob/main/z80.h

#include <CoreMinimal.h>
#include "Interface_CPU_Z80.h"
#include "Devices/Device.h"

struct FInternalRegisters : public FRegisters
{
    bool l1;
    bool l2;
    bool l3;
    bool l4;        // end of instruction
    bool l5;
    bool l6;
    bool l7;
    bool l8;
    bool l9;
    bool l10;
    bool l11;
    bool l12;
    bool l13;
    bool l14;
    bool l15;
    bool l16;
    bool l17;
    bool l18;
    bool l19;
    bool l20;
    bool l21;
    bool l22;
    bool l23;
    bool l24;
    bool l25;
    bool l26;
    bool l27;
    bool l28;
    bool l29;
    bool l30;
    bool l31;
    bool l32;
    bool l33;
    bool l34;
    bool l35;
    bool l36;
    bool l37;
    bool l38;
    bool l39;
    bool l40;
    bool l41;
    bool l42;
    bool l43;
    bool l44;
    bool l45;
    bool l46;       // exx
    bool l47;
    bool l48;
    bool l49;       // ex af, af'
    bool l50;
    bool l51;
    bool l52;
    bool l53;
    bool l54;
    bool l55;
    bool l56;
    bool l57;
    bool l58;
    bool l59;
    bool l60;
    bool l61;
    bool l62;
    bool l63;
    bool l64;
    bool l65;
    bool l66;
    bool l67;
    bool l68;
    bool l70;
    bool l71;
    bool l72;
    bool l73;
    bool l76;
    bool l75;
    bool l77;
    bool l78;
    bool l79;
    bool l81;
    bool l82;
    bool l83;
    bool l84;

    bool w1;
    bool w2;
    bool w3;
    bool w4;
    bool w5;
    bool w6;
    bool w7;
    bool w8;
    bool w9;
    bool w10;
    bool w11;
    bool w12;
    bool w13;
    bool w14;
    bool w15;
    bool w16;
    bool w18;
    bool w19;       // accept NMI
    bool w21;
    bool w22;
    bool w23;
    bool w24;
    bool w25;
    bool w26;
    bool w27;
    bool w28;
    bool w30;
    bool w31;
    bool w32;
    bool w33;
    bool w34;
    bool w35;
    bool w36;
    bool w37;
    bool w38;
    bool w39;
    bool w40;
    bool w41;       // t3
    bool w42;
    bool w43;
    bool w44;
    bool w45;
    bool w46;
    bool w47;
    bool w48;
    bool w49;
    bool w50;
    bool w51;
    bool w52;
    bool w53;
    bool w54;
    bool w55;
    bool w56;
    bool w57;
    bool w58;
    bool w59;
    bool w60;
    bool w61;       // t6
    bool w62;
    bool w63;
    bool w65;
    bool w66;
    bool w67;
    bool w68;       // t5
    bool w69;
    bool w71;
    bool w73;
    bool w74;
    bool w75;
    bool w76;
    bool w77;
    bool w78;
    bool w79;
    bool w80;
    bool w81;
    bool w82;       // alu opcode
    bool w83;
    bool w84;
    bool w85;
    bool w86;
    bool w87;
    bool w88;
    bool w89;
    bool w90;
    bool w91;
    bool w92;       // misc opcode
    bool w93;
    bool w94;
    bool w95;
    bool w96;       // bit opcode
    bool w97;
    bool w98;
    bool w99;
    bool w100;      // ix/iy prefix?

    bool w101;
    bool w102;
    bool w103;
    bool w104;
    bool w105;
    bool w106;
    bool w107;
    bool w109;      // t4
    bool w110;      // t1
    bool w111;
    bool w112;
    bool w113;
    bool w115;      // m6
    bool w116;
    bool w114;      // t2
    bool w117;
    bool w118;
    bool w120;      // m2
    bool w121;      // m5
    bool w122;
    bool w123;      // m4
    bool w125;
    bool w126;
    bool w127;      // m3
    bool w128;
    bool w129;
    bool w130;
    bool w131;      // m1
    bool w132;
    bool w133;
    bool w134;
    bool w135;
    bool w136;
    bool w137;
    bool w138;
    bool w139;
    bool w140;
    bool w141;
    bool w142;
    bool w143;
    bool w144;
    uint8_t w145;
    uint8_t w146;   // bus 1
    uint8_t w147;   // opcode
    bool w148;
    bool w149;
    bool w150;
    bool w151;
    bool w152;
    bool w153;
    bool w154;
    bool w155;
    bool w156;
    bool w157;
    bool w158;
    bool w159;
    bool w160;
    bool w161;
    bool w162;
    bool w163;
    bool w164;
    bool w165;
    bool w166;
    bool w167;
    bool w168;
    bool w169;
    bool w170;
    bool w171;
    bool w172;
    bool w173;
    bool w174;
    bool w175;
    bool w176;
    bool w177;
    bool w178;
    bool w179;
    bool w180;
    bool w181;
    bool w182;
    bool w183;
    bool w184;
    bool w185;
    bool w186;
    bool w187;
    bool w188;
    bool w189;
    bool w190;
    bool w191;
    bool w192;
    bool w193;
    bool w194;
    bool w195;
    bool w196;
    bool w197;
    bool w198;
    bool w199;
    bool w200;

    bool w201;
    bool w202;
    bool w203;
    bool w204;
    bool w205;
    bool w206;
    bool w207;
    bool w208;
    bool w209;
    bool w210;
    bool w211;
    bool w212;
    bool w213;
    bool w214;
    bool w215;
    bool w216;
    bool w217;
    bool w218;
    bool w219;
    bool w220;
    bool w221;
    bool w222;
    bool w223;
    bool w224;
    bool w225;
    bool w226;
    bool w227;
    bool w228;
    bool w229;
    bool w230;
    bool w231;
    bool w232;
    bool w233;
    bool w234;
    bool w235;
    bool w236;
    bool w237;
    bool w238;
    bool w239;
    bool w240;
    bool w241;
    bool w242;
    bool w243;
    bool w244;
    bool w245;
    bool w246;
    bool w247;
    bool w248;
    bool w249;
    bool w250;
    bool w251;
    bool w252;
    bool w253;
    bool w254;
    bool w255;
    bool w256;
    bool w257;
    bool w258;
    bool w259;
    bool w260;
    bool w261;
    bool w262;
    bool w263;
    bool w264;
    bool w265;
    bool w266;
    bool w267;
    bool w268;
    bool w269;
    bool w270;
    bool w271;
    bool w272;
    bool w273;
    bool w274;
    bool w275;
    bool w276;
    bool w277;
    bool w278;
    bool w279;
    bool w280;
    bool w281;
    bool w282;
    bool w283;
    bool w284;
    bool w285;
    bool w286;
    bool w287;
    bool w288;
    bool w289;
    bool w290;
    bool w291;
    bool w292;
    bool w293;
    bool w294;
    bool w295;
    bool w296;
    bool w297;
    bool w298;
    bool w299;
    bool w300;

    bool w301;
    bool w302;
    bool w303;
    bool w304;
    bool w305;
    bool w306;
    bool w307;
    bool w308;
    bool w309;
    bool w310;
    bool w311;
    bool w312;
    bool w313;
    bool w314;      // wz ?
    bool w315;
    bool w316;
    bool w317;
    bool w318;
    bool w319;      // sp
    bool w320;
    bool w321;
    bool w322;
    bool w323;
    bool w324;
    bool w325;
    bool w326;
    bool w327;      // trigger change rigister pair (exx)
    bool w328;
    bool w329;
    bool w330;
    bool w331;
    bool w332;
    bool w333;
    bool w334;
    bool w335;
    bool w336;
    bool w337;
    bool w338;
    bool w339;
    bool w340;      // iy
    bool w341;
    bool w342;      // ix
    bool w343;
    bool w344;
    bool w345;
    bool w346;
    bool w347;
    bool w348;      // de'
    bool w349;
    bool w350;      // de
    bool w351;
    bool w352;      // hl'
    bool w353;      // hl
    bool w354;      // bc
    bool w355;      // bc'
    bool w356;
    bool w357;
    bool w358;
    bool w359;
    bool w360;
    bool w361;      // trigger change register pair (af/af')
    bool w362;
    bool w363;      // af'
    bool w364;      // af
    bool w365;
    bool w366;
    bool w367;
    bool w368;
    bool w369;
    bool w370;
    bool w371;
    bool w372;
    bool w373;
    bool w374;
    bool w375;
    bool w376;
    bool w377;
    bool w378;
    bool w379;
    bool w380;
    bool w381;
    bool w382;
    bool w383;
    bool w384;
    bool w385;
    bool w386;
    bool w387;
    bool w388;
    bool w389;
    bool w390;
    bool w391;
    bool w392;
    bool w393;
    bool w394;
    bool w395;
    bool w396;
    bool w397;
    bool w398;
    bool w399;
    bool w400;

    bool w401;
    bool w403;
    bool w402;
    bool w404;
    bool w405;
    bool w406;
    bool w407;
    bool w408;
    bool w409;
    bool w410;
    bool w411;
    bool w412;
    bool w413;
    bool w414;
    bool w415;
    bool w416;
    bool w417;
    bool w418;
    bool w419;
    bool w420;
    bool w421;
    bool w422;
    bool w423;
    bool w424;
    bool w425;
    bool w426;
    bool w427;
    bool w428;
    bool w429;
    bool w430;
    bool w431;
    bool w432;
    bool w433;
    bool w434;
    bool w435;
    bool w436;
    bool w437;
    bool w438;
    bool w439;
    bool w440;
    bool w441;
    bool w442;
    bool w443;
    bool w444;
    bool w445;
    bool w446;
    bool w448;
    bool w449;
    bool w450;
    bool w452;
    bool w453;
    bool w454;
    bool w455;
    bool w456;
    bool w457;
    bool w458;
    bool w459;
    bool w460;
    bool w461;
    bool w462;
    bool w463;
    bool w464;
    bool w465;
    bool w466;
    bool w467;
    bool w468;
    bool w469;
    bool w470;
    bool w471;
    bool w472;
    bool w473;
    bool w474;
    bool w475;
    bool w476;
    bool w477;
    bool w479;
    bool w480;
    bool w481;
    bool w483;
    uint8_t w484;   // bus 2
    bool w485;
    bool w486;
    bool w487;
    bool w490;
    bool w491;
    bool w492;
    bool w493;
    bool w494;
    bool w495;
    uint8_t w496;   // alu bus
    uint8_t w497;
    uint8_t w498;
    uint8_t w499;

    uint8_t w500;
    bool w501;
    bool w502;
    uint8_t w503;
    uint8_t w504;
    bool w505;
    bool w506;
    bool w507;
    bool w508;
    uint8_t w510;
    uint8_t w511;
    uint8_t w512;
    uint8_t w513;
    uint16_t w514;
    uint16_t w515;
    bool w516;
    bool w517;
    bool w518;
    bool w519;
    uint16_t w520;
    uint16_t w521;
    uint16_t w522;
    uint16_t w523;
    uint16_t w525;
    uint16_t w526;
    uint16_t w527;
    uint16_t w528;
    uint16_t w529;
    bool w524;
    bool w530;
    bool w531;

    bool halt;
    bool pla[99];
    uint16_t pull1[2];
    uint16_t pull2[2];
    uint16_t regs_[12][2];  // af|af'|bc'|bc|hl|hl'|de|de'|ix|iy|sp|wz?
    uint16_t regs2_[2][2];  // pc|ir
    int32_t ix;
	uint8_t bu1, bu2, bu3;
	bool alu_calc;
    //bool o_addr_high;       // сигнал что на ША адресс
    //bool ext_data_o_high;   // сигнал что на ШД данные

	uint32_t CC;			// counter of clock cycles
};

class FCPU_Z80_NMOS : public FDevice, public ICPU_Z80
{
	using ThisClass = FCPU_Z80_NMOS;
public:
	FCPU_Z80_NMOS();
    virtual ~FCPU_Z80_NMOS() = default;

	virtual void Tick() override;
	virtual void Reset() override;
    virtual FRegisters GetRegisters() const override;

private:
    void ClkLatches(bool bClk);
    void ResetLogic(bool bClk);
    void StateLogic(bool bClk);
    void InterruptLogic(bool bClk);
    void IOLogic(bool bClk);
    void BusLogic(bool bClk);
    void OpcodeFetch(bool bClk);
    void HaltLogic(bool bClk);
    void InterruptFlipFlops(bool bClk);
    void OpcodeDecode(bool bClk);
    void InterruptLogic2(bool bClk);
    void RegistersLogic(bool bClk);
    void AluControlLogic(bool bClk);
    void CalcAlu();
    void AluLogic(bool bClk);
    void RegistersLogic2(bool bClk);
    void BusLogic2(bool bClk);
    void AluLogic2(bool bClk);
    void RegistersLogic3(bool bClk);
    void ConditionLogic(bool bClk);
	void Clock();

    FInternalRegisters Registers;
};
