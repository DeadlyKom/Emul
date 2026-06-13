#pragma once

#include <CoreMinimal.h>

namespace CodeGenerator
{
    static constexpr uint16_t ZX_SCREEN_BASE = 0x4000;
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
    };

    struct FCandidate
    {
        EOpKind Kind = EOpKind::ByteAbsA;

        int32_t StartOffset = 0;
        int32_t EndOffset = 0;

        uint16_t StartAddr = 0;

        int32_t WriteBytes = 0;
        int32_t Cycles = 0;
        int32_t CodeBytes = 0;

        uint8_t ByteValue = 0;
        uint16_t WordValue = 0;

        // Для первой версии оптимизатора все кандидаты линейные:
        // покрывают [StartOffset, EndOffset).
        bool Linear = true;
    };

    struct FAnalysis
    {
        std::vector<uint8_t> Data;
        std::vector<uint8_t> Dirty;
        std::vector<FCandidate> Candidates;

        // StartsAt[Offset] -> ID кандидатов, которые могут начаться с Offset.
        std::vector<std::vector<int32_t>> StartsAt;
    };

    struct FPlan
    {
        std::vector<int32_t> CandidateIds;
        int TotalCycles = 0;
        int TotalCodeBytes = 0;
    };

    struct FOptions
    {
        // Сколько пар максимум перебирать для STACK_BLOCK с каждого Offset.
        // 16 пар = 32 экранных байта.
        // Большой цельный блок тоже добавляется отдельно.
        int32_t MaxStackPairsToEnumerate = 16;

        // Если важнее скорость:
        // CycleWeight = 1000, ByteWeight = 1
        //
        // Если хочешь баланс:
        // CycleWeight = 10, ByteWeight = 1
        //
        // Если хочешь минимальный код:
        // CycleWeight = 1, ByteWeight = 20
        int64_t CycleWeight = 1000;
        int64_t ByteWeight = 1;

        bool PreserveSP = true;
        bool DisableInterruptsForStack = true;

        bool EnableByteCandidates = true;
        bool EnableWordCandidates = true;
        bool EnableStackBlocks = true;
        bool EnableRepeatWords = true;
        bool EnableHorizontalSameByteIncL = true;
    };

    struct FEmitState
    {
        bool bHasA = false;
        uint8_t A = 0;

        bool bHasHL = false;
        uint16_t HL = 0;

        bool bHasSP = false;
        uint16_t SP = 0;
    };

    inline bool IsValidOffset(int Offset)
    {
        return Offset >= 0 && Offset < ZX_SCREEN_SIZE;
    }

    inline uint16_t AddrOf(int Offset)
    {
        return static_cast<uint16_t>(ZX_SCREEN_BASE + Offset);
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

    void AddCandidate(FAnalysis& Analysis, const FCandidate& Candidate);
    bool RangeIsDirty(const std::vector<uint8_t>& Dirty, int32_t Start, int32_t End);

    FCandidate MakeByteAbsA(const std::vector<uint8_t>& Data, int32_t Offset);
    FCandidate MakeByteHLImm(const std::vector<uint8_t>& Data, int32_t Offset);
    FCandidate MakeWordAbsHL(const std::vector<uint8_t>& Data, int32_t Offset);
    FCandidate MakeStackBlock(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t ByteLength);
    FCandidate MakeRepeatWordAbsHL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t PairCount);
    FCandidate MakeRepeatWordStack(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t PairCount);
    FCandidate MakeHorizontalSameByteIncL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length);

    bool BuildAnalysis(const std::vector<uint8_t>& Data, const std::vector<uint8_t>& DirtyMask, const FOptions& Options, FAnalysis& OutAnalysis, std::string& OutError);
    bool OptimizePlan(const FAnalysis& Analysis, const FOptions& Options, FPlan& OutPlan, std::string& OutError);

    void EmitLdA(std::ostringstream& Out, FEmitState& State, uint8_t Value);
    void EmitLdHL(std::ostringstream& Out, FEmitState& State, uint16_t Value);
    void EmitLdSP(std::ostringstream& Out, FEmitState& State, uint16_t Value);
    bool EmitCandidate(std::ostringstream& Out, const FCandidate& Candidate, const std::vector<uint8_t>& Data, FEmitState& State, std::string& OutError);
    bool EmitAsm(const FAnalysis& Analysis, const FPlan& Plan, const FOptions& Options, std::string& OutAsm, std::string& OutError, const std::string& LabelName = "DrawGeneratedScreen");

    void PrintPlanSummary(const FAnalysis& Analysis, const FPlan& Plan);
}
