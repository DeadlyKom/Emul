#pragma once

#include <atomic>
#include <CoreMinimal.h>

namespace CodeGenerator
{
    static constexpr uint16_t ZX_SCREEN_BASE = 0x4000;
    static constexpr uint16_t ZX_SHADOW_SCREEN_BASE = 0xC000;
    static constexpr uint16_t ZX_PRINTER_BUFFER_BASE = 0x5B00;
    static constexpr uint16_t ZX_STACK_TRAMPOLINE_TOP = ZX_PRINTER_BUFFER_BASE + 2; // touches #5B00..#5B03
    static constexpr int32_t ZX_PIXEL_SIZE = 0x1800;
    static constexpr int32_t ZX_ATTRIBUTE_SIZE = 0x0300;
    static constexpr int32_t ZX_SCREEN_SIZE = 0x1B00;
    static constexpr uint16_t ZX_SCREEN_END = ZX_SCREEN_BASE + ZX_SCREEN_SIZE; // #5B00

    enum class EOpKind
    {
        ByteAbsA,               // LD A,n / LD (nn),A
        ByteHLImm,              // LD HL,nn / LD (HL),n
        WordAbsHL,              // LD HL,word / LD (nn),HL

        StackBlock,             // LD SP,end / LD HL,word / PUSH HL...
        RepeatWordAbsHL,        // LD HL,word / LD (addr),HL / LD (addr+2),HL...
        RepeatWordStack,        // LD SP,end / LD HL,word / PUSH HL...

        HorizontalSameByteIncL, // LD HL,addr / LD A,n / LD (HL),A / INC L...

        VerticalBytesIncH,      // LD HL,addr / LD (HL),n / INC H...
        VerticalSameByteIncH,   // LD HL,addr / LD A,n / LD (HL),A / INC H...
        VerticalRepeatWordAbsHL,// LD HL,word / LD (addr),HL / LD (addr+#100),HL...

        HorizontalSameByteDecL,    // LD HL,addr / LD A,n / LD (HL),A / DEC L...
        HorizontalSameByteRegIncL, // LD HL,addr / LD (HL),B|C|D|E / INC L...
        HorizontalSameByteRegDecL, // LD HL,addr / LD (HL),B|C|D|E / DEC L...
        VerticalBytesDecH,         // LD HL,addr / LD (HL),n / DEC H...
        VerticalSameByteDecH,      // LD HL,addr / LD A,n / LD (HL),A / DEC H...
        VerticalSameByteRegIncH,   // LD HL,addr / LD (HL),B|C|D|E / INC H...
        VerticalSameByteRegDecH,   // LD HL,addr / LD (HL),B|C|D|E / DEC H...
        ZXColumnBytes,             // LD HL,addr / LD (HL),n / next ZX raster row...
        ZXColumnSameByte,          // LD HL,addr / LD A,n / LD (HL),A / next ZX raster row...
        ZXColumnSameByteReg,       // LD HL,addr / LD (HL),B|C|D|E / next ZX raster row...
    };

    struct FResult
    {
        bool bSuccess;
        std::string Error;
        std::string AsmCode;
        std::vector<uint8_t> ByteCode;
        int32_t OperationCount;
        int32_t Cycles;
        int32_t CodeBytes;
        int32_t DirtyBytes;

        FResult()
            : bSuccess(false)
            , OperationCount(0)
            , Cycles(0)
            , CodeBytes(0)
            , DirtyBytes(0)
        {
        }
    };

    struct FCandidate
    {
        EOpKind Kind;

        int32_t StartOffset;
        int32_t EndOffset;

        uint16_t StartAddr;

        int32_t WriteBytes;
        int32_t Cycles;
        int32_t CodeBytes;
        int32_t FrequencyScore;

        uint8_t ByteValue;
        uint16_t WordValue;
        char RegisterName;
        bool RequiresB;
        bool RequiresC;
        bool RequiresD;
        bool RequiresE;

        // Для первой версии оптимизатора все кандидаты линейные:
        // покрывают [StartOffset, EndOffset).
        bool Linear;

        // Для нелинейных кандидатов: точный список байтов, которые будут записаны.
        // Для линейных кандидатов может быть пустым, покрытие задается [StartOffset, EndOffset).
        std::vector<int32_t> CoveredOffsets;
        int32_t Stride;
        int32_t Count;
        int32_t Width;

        FCandidate()
            : Kind(EOpKind::ByteAbsA)
            , StartOffset(0)
            , EndOffset(0)
            , StartAddr(0)
            , WriteBytes(0)
            , Cycles(0)
            , CodeBytes(0)
            , FrequencyScore(0)
            , ByteValue(0)
            , WordValue(0)
            , RegisterName(0)
            , RequiresB(false)
            , RequiresC(false)
            , RequiresD(false)
            , RequiresE(false)
            , Linear(true)
            , Stride(1)
            , Count(0)
            , Width(1)
        {}
    };

    struct FAnalysis
    {
        std::vector<uint8_t> Data;
        std::vector<uint8_t> Dirty;
        std::vector<int32_t> ByteFrequency;
        std::vector<int32_t> WordFrequency;
        std::vector<FCandidate> Candidates;
        bool bHasPreferredBC;
        uint8_t PreferredB;
        uint8_t PreferredC;
        bool bHasPreferredDE;
        uint8_t PreferredD;
        uint8_t PreferredE;
        uint16_t ScreenBaseAddress;

        // StartsAt[Offset] -> ID кандидатов, которые могут начаться с Offset.
        std::vector<std::vector<int32_t>> StartsAt;

        FAnalysis()
            : bHasPreferredBC(false)
            , PreferredB(0)
            , PreferredC(0)
            , bHasPreferredDE(false)
            , PreferredD(0)
            , PreferredE(0)
            , ScreenBaseAddress(ZX_SCREEN_BASE)
        {
        }
    };

    struct FPlan
    {
        std::vector<int32_t> CandidateIds;
        int TotalCycles;
        int TotalCodeBytes;
        bool UsesB;
        bool UsesC;
        bool UsesD;
        bool UsesE;
        bool UsesStack;

        FPlan()
            : TotalCycles(0)
            , TotalCodeBytes(0)
            , UsesB(false)
            , UsesC(false)
            , UsesD(false)
            , UsesE(false)
            , UsesStack(false)
        {
        }
    };

    struct FOptions
    {
        // Сколько пар максимум перебирать для STACK_BLOCK с каждого Offset.
        // 16 пар = 32 экранных байта.
        // Большой цельный блок тоже добавляется отдельно.
        int32_t MaxStackPairsToEnumerate;
        int32_t NonLinearBeamWidth;
        int32_t MaxNonLinearCandidatesToEvaluatePerPass;

        // Если важнее скорость:
        // CycleWeight = 1000, ByteWeight = 1
        //
        // Если хочешь баланс:
        // CycleWeight = 10, ByteWeight = 1
        //
        // Если хочешь минимальный код:
        // CycleWeight = 1, ByteWeight = 200
        int64_t CycleWeight;
        int64_t ByteWeight;

        bool PreserveSP;
        bool DisableInterruptsForStack;
        uint16_t StackTopAddress;
        uint16_t ScreenBaseAddress;

        bool EnableByteCandidates;
        bool EnableWordCandidates;
        bool EnableStackBlocks;
        bool EnableRepeatWords;
        bool EnableHorizontalSameByteIncL;
        bool EnableVerticalCandidates;
        bool EnableReverseDirections;
        bool EnableRegisterConstants;

        FOptions()
            : MaxStackPairsToEnumerate(16)
            , NonLinearBeamWidth(4)
            , MaxNonLinearCandidatesToEvaluatePerPass(96)
            , CycleWeight(1000)
            , ByteWeight(1)
            , PreserveSP(true)
            , DisableInterruptsForStack(true)
            , StackTopAddress(ZX_STACK_TRAMPOLINE_TOP)
            , ScreenBaseAddress(ZX_SCREEN_BASE)
            , EnableByteCandidates(true)
            , EnableWordCandidates(true)
            , EnableStackBlocks(true)
            , EnableRepeatWords(true)
            , EnableHorizontalSameByteIncL(true)
            , EnableVerticalCandidates(true)
            , EnableReverseDirections(true)
            , EnableRegisterConstants(true)
        {
        }
    };

    struct FEmitState
    {
        bool bHasA;
        uint8_t A;

        bool bHasHL;
        uint16_t HL;
        bool bHasH;
        bool bHasL;

        bool bHasBC;
        uint16_t BC;
        bool bHasB;
        bool bHasC;

        bool bHasDE;
        uint16_t DE;
        bool bHasD;
        bool bHasE;

        bool bHasSP;
        uint16_t SP;
        uint16_t ScreenBaseAddress;

        bool bHasPreferredBC;
        uint16_t PreferredBC;
        bool bHasPreferredDE;
        uint16_t PreferredDE;

        FEmitState()
            : bHasA(false)
            , A(0)
            , bHasHL(false)
            , HL(0)
            , bHasH(false)
            , bHasL(false)
            , bHasBC(false)
            , BC(0)
            , bHasB(false)
            , bHasC(false)
            , bHasDE(false)
            , DE(0)
            , bHasD(false)
            , bHasE(false)
            , bHasSP(false)
            , SP(0)
            , ScreenBaseAddress(ZX_SCREEN_BASE)
            , bHasPreferredBC(false)
            , PreferredBC(0)
            , bHasPreferredDE(false)
            , PreferredDE(0)
        {
        }
    };

    struct FEmitOutput
    {
        std::ostringstream Preview;
        std::vector<uint8_t> Code;
        int32_t Cycles;

        FEmitOutput()
            : Cycles(0)
        {
        }
    };

    struct FProgressInfo
    {
        std::atomic<bool>* CancelRequested;
        std::atomic<int32_t>* Current;
        std::atomic<int32_t>* Total;

        FProgressInfo()
            : CancelRequested(nullptr)
            , Current(nullptr)
            , Total(nullptr)
        {
        }
    };

    inline bool IsValidOffset(int Offset)
    {
        return Offset >= 0 && Offset < ZX_SCREEN_SIZE;
    }

    inline uint16_t AddrOf(int Offset)
    {
        return static_cast<uint16_t>(ZX_SCREEN_BASE + Offset);
    }

    inline uint16_t AddrOf(int Offset, uint16_t ScreenBaseAddress)
    {
        return static_cast<uint16_t>(ScreenBaseAddress + Offset);
    }

    inline uint16_t ReadWordLE(const std::vector<uint8_t>& Data, int32_t Offset)
    {
        return static_cast<uint16_t>(Data[Offset] | (Data[Offset + 1] << 8));
    }

    inline int64_t ScoreCandidate(const FCandidate& Candidate, const FOptions& Options)
    {
        return static_cast<int64_t>(Candidate.Cycles) * Options.CycleWeight + static_cast<int64_t>(Candidate.CodeBytes) * Options.ByteWeight;
    }


    const char* ToString(EOpKind Kind);
    std::string Hex8(uint8_t Value);
    std::string Hex16(uint16_t Value);
    std::string MakeFrameLabelName(int32_t FrameIndex);

    void AddCandidate(FAnalysis& Analysis, const FCandidate& Candidate);
    bool RangeIsDirty(const std::vector<uint8_t>& Dirty, int32_t Start, int32_t End);
    bool CandidateIsDirty(const std::vector<uint8_t>& Dirty, const FCandidate& Candidate);
    bool CandidateCoversOffset(const FCandidate& Candidate, int32_t Offset);

    FCandidate MakeByteAbsA(const std::vector<uint8_t>& Data, int32_t Offset);
    FCandidate MakeByteHLImm(const std::vector<uint8_t>& Data, int32_t Offset);
    FCandidate MakeWordAbsHL(const std::vector<uint8_t>& Data, int32_t Offset);
    FCandidate MakeStackBlock(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t ByteLength);
    FCandidate MakeRepeatWordAbsHL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t PairCount);
    FCandidate MakeRepeatWordStack(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t PairCount);
    FCandidate MakeHorizontalSameByteIncL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length);
    FCandidate MakeHorizontalSameByteDecL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length);
    FCandidate MakeHorizontalSameByteRegIncL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length, char RegisterName);
    FCandidate MakeHorizontalSameByteRegDecL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length, char RegisterName);
    FCandidate MakeVerticalBytesIncH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count);
    FCandidate MakeVerticalSameByteIncH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count);
    FCandidate MakeVerticalRepeatWordAbsHL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count);
    FCandidate MakeVerticalBytesDecH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count);
    FCandidate MakeVerticalSameByteDecH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count);
    FCandidate MakeVerticalSameByteRegIncH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count, char RegisterName);
    FCandidate MakeVerticalSameByteRegDecH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count, char RegisterName);
    FCandidate MakeZXColumnBytes(const std::vector<uint8_t>& Data, int32_t ByteX, int32_t StartY, int32_t Count);
    FCandidate MakeZXColumnSameByte(const std::vector<uint8_t>& Data, int32_t ByteX, int32_t StartY, int32_t Count);
    FCandidate MakeZXColumnSameByteReg(const std::vector<uint8_t>& Data, int32_t ByteX, int32_t StartY, int32_t Count, char RegisterName);

    bool BuildAnalysis(const std::vector<uint8_t>& Data, const std::vector<uint8_t>& DirtyMask, const FOptions& Options, FAnalysis& OutAnalysis, std::string& OutError, const FProgressInfo* Progress = nullptr);
    bool OptimizePlan(const FAnalysis& Analysis, const FOptions& Options, FPlan& OutPlan, std::string& OutError, const FProgressInfo* Progress = nullptr);

    void EmitByte(FEmitOutput& Out, uint8_t Value);
    void EmitWordLE(FEmitOutput& Out, uint16_t Value);
    void PatchWordLE(FEmitOutput& Out, size_t Offset, uint16_t Value);
    void EmitLD_A(FEmitOutput& Out, FEmitState& State, uint8_t Value);
    void EmitLD_HL(FEmitOutput& Out, FEmitState& State, uint16_t Value);
    void EmitLD_BC(FEmitOutput& Out, FEmitState& State, uint16_t Value);
    void EmitLD_DE(FEmitOutput& Out, FEmitState& State, uint16_t Value);
    void EmitLD_SP(FEmitOutput& Out, FEmitState& State, uint16_t Value);
    bool EmitCandidate(FEmitOutput& Out, const FCandidate& Candidate, const std::vector<uint8_t>& Data, FEmitState& State, std::string& OutError);
    bool EmitAsm(const FAnalysis& Analysis, const FPlan& Plan, const FOptions& Options, std::string& OutAsm, std::vector<uint8_t>& OutCode, int32_t& OutCycles, std::string& OutError, const std::string& LabelName, const FProgressInfo* Progress = nullptr);

    void PrintPlanSummary(const FAnalysis& Analysis, const FPlan& Plan);
}
