#include "CodeGenerator.h"

#include <algorithm>
#include <cstring>

namespace
{
    inline bool ProgressCancelled(const CodeGenerator::FProgressInfo* Progress)
    {
        return Progress && Progress->CancelRequested && Progress->CancelRequested->load(std::memory_order_relaxed);
    }

    inline void ProgressSet(const CodeGenerator::FProgressInfo* Progress, int32_t Current, int32_t Total)
    {
        if (!Progress)
        {
            return;
        }

        const int32_t SafeTotal = Total > 0 ? Total : 1;

        if (Progress->Current)
        {
            Progress->Current->store(Current, std::memory_order_relaxed);
        }
        if (Progress->Total)
        {
            Progress->Total->store(SafeTotal, std::memory_order_relaxed);
        }
    }
}

const char* CodeGenerator::ToString(EOpKind Kind)
{
    switch (Kind)
    {
    case EOpKind::ByteAbsA:                 return "BYTE_ABS_A";
    case EOpKind::ByteHLImm:                return "BYTE_HL_IMM";
    case EOpKind::WordAbsHL:                return "WORD_ABS_HL";
    case EOpKind::StackBlock:               return "STACK_BLOCK";
    case EOpKind::RepeatWordAbsHL:          return "REPEAT_WORD_ABS_HL";
    case EOpKind::RepeatWordStack:          return "REPEAT_WORD_STACK";
    case EOpKind::HorizontalSameByteIncL:   return "HORIZONTAL_SAME_BYTE_INC_L";
    case EOpKind::VerticalBytesIncH:        return "VERTICAL_BYTES_INC_H";
    case EOpKind::VerticalSameByteIncH:     return "VERTICAL_SAME_BYTE_INC_H";
    case EOpKind::VerticalRepeatWordAbsHL:  return "VERTICAL_REPEAT_WORD_ABS_HL";
    case EOpKind::HorizontalSameByteDecL:   return "HORIZONTAL_SAME_BYTE_DEC_L";
    case EOpKind::HorizontalSameByteRegIncL:return "HORIZONTAL_SAME_BYTE_REG_INC_L";
    case EOpKind::HorizontalSameByteRegDecL:return "HORIZONTAL_SAME_BYTE_REG_DEC_L";
    case EOpKind::VerticalBytesDecH:        return "VERTICAL_BYTES_DEC_H";
    case EOpKind::VerticalSameByteDecH:     return "VERTICAL_SAME_BYTE_DEC_H";
    case EOpKind::VerticalSameByteRegIncH:  return "VERTICAL_SAME_BYTE_REG_INC_H";
    case EOpKind::VerticalSameByteRegDecH:  return "VERTICAL_SAME_BYTE_REG_DEC_H";
    default:                                return "UNKNOWN";
    }
}

std::string CodeGenerator::Hex8(uint8_t Value)
{
    std::ostringstream ss;
    ss << "#"
        << std::uppercase << std::hex
        << std::setw(2) << std::setfill('0')
        << static_cast<int>(Value);
    return ss.str();
}

std::string CodeGenerator::Hex16(uint16_t Value)
{
    std::ostringstream ss;
    ss << "#"
        << std::uppercase << std::hex
        << std::setw(4) << std::setfill('0')
        << static_cast<int>(Value);
    return ss.str();
}

std::string CodeGenerator::MakeFrameLabelName(int32_t FrameIndex)
{
    return std::format("DrawFrame_{}:", FrameIndex);
}

void CodeGenerator::AddCandidate(FAnalysis& Analysis, const FCandidate& Candidate)
{
    if (Candidate.StartOffset < 0 || Candidate.StartOffset >= ZX_SCREEN_SIZE)
    {
        return;
    }

    if (Candidate.EndOffset <= Candidate.StartOffset || Candidate.EndOffset > ZX_SCREEN_SIZE)
    {
        return;
    }

    const int32_t ID = static_cast<int32_t>(Analysis.Candidates.size());
    Analysis.Candidates.push_back(Candidate);
    Analysis.StartsAt[Candidate.StartOffset].push_back(ID);
}

bool CodeGenerator::RangeIsDirty(const std::vector<uint8_t>& Dirty, int32_t Start, int32_t End)
{
    if (Start < 0 || End > ZX_SCREEN_SIZE || Start >= End)
    {
        return false;
    }

    for (int32_t i = Start; i < End; ++i)
    {
        if (!Dirty[i])
        {
            return false;
        }
    }

    return true;
}

bool CodeGenerator::CandidateIsDirty(const std::vector<uint8_t>& Dirty, const FCandidate& Candidate)
{
    if (Candidate.Linear)
    {
        return RangeIsDirty(Dirty, Candidate.StartOffset, Candidate.EndOffset);
    }

    if (Candidate.CoveredOffsets.empty())
    {
        return false;
    }

    for (int32_t Offset : Candidate.CoveredOffsets)
    {
        if (!IsValidOffset(Offset) || !Dirty[Offset])
        {
            return false;
        }
    }

    return true;
}

bool CodeGenerator::CandidateCoversOffset(const FCandidate& Candidate, int32_t Offset)
{
    if (Candidate.Linear)
    {
        return Offset >= Candidate.StartOffset && Offset < Candidate.EndOffset;
    }

    return std::find(Candidate.CoveredOffsets.begin(), Candidate.CoveredOffsets.end(), Offset) != Candidate.CoveredOffsets.end();
}

CodeGenerator::FCandidate CodeGenerator::MakeByteAbsA(const std::vector<uint8_t>& Data, int32_t Offset)
{
    FCandidate C;
    C.Kind = EOpKind::ByteAbsA;
    C.StartOffset = Offset;
    C.EndOffset = Offset + 1;
    C.StartAddr = AddrOf(Offset);
    C.WriteBytes = 1;
    C.ByteValue = Data[Offset];

    // LD A, n       7 / 2 bytes
    // LD (nn), A   13 / 3 bytes
    C.Cycles = 20;
    C.CodeBytes = 5;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeByteHLImm(const std::vector<uint8_t>& Data, int32_t Offset)
{
    FCandidate C;
    C.Kind = EOpKind::ByteHLImm;
    C.StartOffset = Offset;
    C.EndOffset = Offset + 1;
    C.StartAddr = AddrOf(Offset);
    C.WriteBytes = 1;
    C.ByteValue = Data[Offset];

    // LD HL, nn     10 / 3 bytes
    // LD (HL), n    10 / 2 bytes
    C.Cycles = 20;
    C.CodeBytes = 5;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeWordAbsHL(const std::vector<uint8_t>& Data, int32_t Offset)
{
    FCandidate C;
    C.Kind = EOpKind::WordAbsHL;
    C.StartOffset = Offset;
    C.EndOffset = Offset + 2;
    C.StartAddr = AddrOf(Offset);
    C.WriteBytes = 2;
    C.WordValue = ReadWordLE(Data, Offset);

    // LD HL, nn       10 / 3 bytes
    // LD (nn), HL     16 / 3 bytes
    C.Cycles = 26;
    C.CodeBytes = 6;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeStackBlock(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t ByteLength)
{
    FCandidate C;
    C.Kind = EOpKind::StackBlock;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + ByteLength;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = ByteLength;

    // LD SP, End
    C.Cycles = 10;
    C.CodeBytes = 3;

    bool bHasHL = false;
    uint16_t CurrentHL = 0;

    // PUSH пишет назад, поэтому пары читаем с конца блока.
    for (int32_t Pos = StartOffset + ByteLength - 2; Pos >= StartOffset; Pos -= 2)
    {
        const uint16_t Word = ReadWordLE(Data, Pos);
        if (!bHasHL || CurrentHL != Word)
        {
            // LD HL, nn
            C.Cycles += 10;
            C.CodeBytes += 3;

            CurrentHL = Word;
            bHasHL = true;
        }

        // PUSH HL
        C.Cycles += 11;
        C.CodeBytes += 1;
    }

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeRepeatWordAbsHL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t PairCount)
{
    FCandidate C;
    C.Kind = EOpKind::RepeatWordAbsHL;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + PairCount * 2;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = PairCount * 2;
    C.WordValue = ReadWordLE(Data, StartOffset);

    // LD HL, Word
    // PairCount * LD (nn), HL
    C.Cycles = 10 + PairCount * 16;
    C.CodeBytes = 3 + PairCount * 3;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeRepeatWordStack(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t PairCount)
{
    FCandidate C;
    C.Kind = EOpKind::RepeatWordStack;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + PairCount * 2;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = PairCount * 2;
    C.WordValue = ReadWordLE(Data, StartOffset);

    // LD SP, End
    // LD HL, Word
    // PairCount * PUSH HL
    C.Cycles = 10 + 10 + PairCount * 11;
    C.CodeBytes = 3 + 3 + PairCount;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeHorizontalSameByteIncL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length)
{
    FCandidate C;
    C.Kind = EOpKind::HorizontalSameByteIncL;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + Length;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Length;
    C.ByteValue = Data[StartOffset];

    // LD HL, addr       10 / 3
    // LD A, n            7 / 2
    // LD (HL), A         7 / 1
    // далее:
    // INC L              4 / 1
    // LD (HL), A         7 / 1
    C.Cycles = 10 + 7 + 7 + (Length - 1) * (4 + 7);
    C.CodeBytes = 3 + 2 + 1 + (Length - 1) * 2;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeHorizontalSameByteDecL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length)
{
    FCandidate C = MakeHorizontalSameByteIncL(Data, StartOffset - Length + 1, Length);
    C.Kind = EOpKind::HorizontalSameByteDecL;
    C.StartOffset = StartOffset - Length + 1;
    C.EndOffset = StartOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.Stride = -1;
    C.Count = Length;
    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeHorizontalSameByteRegIncL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length, char RegisterName)
{
    FCandidate C;
    C.Kind = EOpKind::HorizontalSameByteRegIncL;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + Length;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Length;
    C.ByteValue = Data[StartOffset];
    C.Count = Length;
    C.Stride = 1;
    C.RegisterName = RegisterName;
    C.RequiresBC = true;
    C.Count = Length;
    C.Stride = 1;

    // LD HL,addr / LD (HL),r / INC L...
    C.Cycles = 10 + 7 + (Length - 1) * (4 + 7);
    C.CodeBytes = 3 + 1 + (Length - 1) * 2;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeHorizontalSameByteRegDecL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Length, char RegisterName)
{
    FCandidate C = MakeHorizontalSameByteRegIncL(Data, StartOffset - Length + 1, Length, RegisterName);
    C.Kind = EOpKind::HorizontalSameByteRegDecL;
    C.StartOffset = StartOffset - Length + 1;
    C.EndOffset = StartOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.Stride = -1;
    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalBytesIncH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count)
{
    FCandidate C;
    C.Kind = EOpKind::VerticalBytesIncH;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Count;
    C.Linear = false;
    C.Stride = 0x0100;
    C.Count = Count;
    C.Width = 1;

    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(StartOffset + i * C.Stride);
    }

    // LD HL, addr / LD (HL), n / INC H...
    C.Cycles = 10 + Count * 10 + (Count - 1) * 4;
    C.CodeBytes = 3 + Count * 2 + (Count - 1);

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalSameByteIncH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count)
{
    FCandidate C;
    C.Kind = EOpKind::VerticalSameByteIncH;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Count;
    C.ByteValue = Data[StartOffset];
    C.Linear = false;
    C.Stride = 0x0100;
    C.Count = Count;
    C.Width = 1;

    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(StartOffset + i * C.Stride);
    }

    // LD HL, addr / LD A, n / LD (HL), A / INC H...
    C.Cycles = 10 + 7 + 7 + (Count - 1) * (4 + 7);
    C.CodeBytes = 3 + 2 + 1 + (Count - 1) * 2;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalRepeatWordAbsHL(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count)
{
    FCandidate C;
    C.Kind = EOpKind::VerticalRepeatWordAbsHL;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + 2;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Count * 2;
    C.WordValue = ReadWordLE(Data, StartOffset);
    C.Linear = false;
    C.Stride = 0x0100;
    C.Count = Count;
    C.Width = 2;

    C.CoveredOffsets.reserve(Count * 2);
    for (int32_t i = 0; i < Count; ++i)
    {
        const int32_t Offset = StartOffset + i * C.Stride;
        C.CoveredOffsets.push_back(Offset);
        C.CoveredOffsets.push_back(Offset + 1);
    }

    // LD HL, word / LD (nn), HL...
    C.Cycles = 10 + Count * 16;
    C.CodeBytes = 3 + Count * 3;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalBytesDecH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count)
{
    const int32_t TopOffset = StartOffset - (Count - 1) * 0x0100;
    FCandidate C;
    C.Kind = EOpKind::VerticalBytesDecH;
    C.StartOffset = TopOffset;
    C.EndOffset = TopOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Count;
    C.Linear = false;
    C.Stride = -0x0100;
    C.Count = Count;
    C.Width = 1;

    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(StartOffset + i * C.Stride);
    }

    C.Cycles = 10 + Count * 10 + (Count - 1) * 4;
    C.CodeBytes = 3 + Count * 2 + (Count - 1);

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalSameByteDecH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count)
{
    const int32_t TopOffset = StartOffset - (Count - 1) * 0x0100;
    FCandidate C;
    C.Kind = EOpKind::VerticalSameByteDecH;
    C.StartOffset = TopOffset;
    C.EndOffset = TopOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Count;
    C.ByteValue = Data[StartOffset];
    C.Linear = false;
    C.Stride = -0x0100;
    C.Count = Count;
    C.Width = 1;

    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(StartOffset + i * C.Stride);
    }

    C.Cycles = 10 + 7 + 7 + (Count - 1) * (4 + 7);
    C.CodeBytes = 3 + 2 + 1 + (Count - 1) * 2;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalSameByteRegIncH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count, char RegisterName)
{
    FCandidate C;
    C.Kind = EOpKind::VerticalSameByteRegIncH;
    C.StartOffset = StartOffset;
    C.EndOffset = StartOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.WriteBytes = Count;
    C.ByteValue = Data[StartOffset];
    C.RegisterName = RegisterName;
    C.RequiresBC = true;
    C.Linear = false;
    C.Stride = 0x0100;
    C.Count = Count;
    C.Width = 1;

    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(StartOffset + i * C.Stride);
    }

    C.Cycles = 10 + 7 + (Count - 1) * (4 + 7);
    C.CodeBytes = 3 + 1 + (Count - 1) * 2;

    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeVerticalSameByteRegDecH(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t Count, char RegisterName)
{
    const int32_t TopOffset = StartOffset - (Count - 1) * 0x0100;
    FCandidate C = MakeVerticalSameByteRegIncH(Data, StartOffset, Count, RegisterName);
    C.Kind = EOpKind::VerticalSameByteRegDecH;
    C.StartOffset = TopOffset;
    C.EndOffset = TopOffset + 1;
    C.StartAddr = AddrOf(StartOffset);
    C.Stride = -0x0100;
    C.CoveredOffsets.clear();
    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(StartOffset + i * C.Stride);
    }
    return C;
}

bool CodeGenerator::BuildAnalysis(const std::vector<uint8_t>& Data, const std::vector<uint8_t>& DirtyMask, const FOptions& Options, FAnalysis& OutAnalysis, std::string& OutError, const FProgressInfo* Progress)
{
    if (Data.size() != ZX_SCREEN_SIZE)
    {
        OutError = "BuildAnalysis: data must contain exactly 6912 bytes";
        return false;
    }

    FAnalysis Analysis;
    Analysis.Data = Data;

    if (DirtyMask.empty())
    {
        Analysis.Dirty.assign(ZX_SCREEN_SIZE, 1);
    }
    else
    {
        if (DirtyMask.size() != ZX_SCREEN_SIZE)
        {
            OutError = "BuildAnalysis: dirtyMask must contain exactly 6912 bytes";
            return false;
        }

        Analysis.Dirty = DirtyMask;
    }

    Analysis.StartsAt.resize(ZX_SCREEN_SIZE);

    ProgressSet(Progress, 0, 100);
    if (ProgressCancelled(Progress))
    {
        OutError = "BuildAnalysis: cancelled";
        return false;
    }

    if (Options.EnableRegisterConstants)
    {
        int32_t Counts[256] = {};
        for (int32_t Offset = 0; Offset < ZX_SCREEN_SIZE; ++Offset)
        {
            if (Analysis.Dirty[Offset])
            {
                ++Counts[Data[Offset]];
            }

            if ((Offset & 0x3FF) == 0 && ProgressCancelled(Progress))
            {
                OutError = "BuildAnalysis: cancelled";
                return false;
            }
        }

        int32_t BestB = 0;
        int32_t BestC = 0;
        for (int32_t Value = 0; Value < 256; ++Value)
        {
            if (Counts[Value] > Counts[BestB])
            {
                BestC = BestB;
                BestB = Value;
            }
            else if (Value != BestB && Counts[Value] > Counts[BestC])
            {
                BestC = Value;
            }
        }

        if (Counts[BestB] > 0)
        {
            Analysis.bHasPreferredBC = true;
            Analysis.PreferredB = static_cast<uint8_t>(BestB);
            Analysis.PreferredC = static_cast<uint8_t>(BestC);
        }
    }

    ProgressSet(Progress, 10, 100);
    if (ProgressCancelled(Progress))
    {
        OutError = "BuildAnalysis: cancelled";
        return false;
    }

    // ------------------------------------------------------------
    // 1. Одиночные байты.
    // ------------------------------------------------------------
    for (int32_t i = 0; i < ZX_SCREEN_SIZE; ++i)
    {
        if (!Analysis.Dirty[i])
        {
            continue;
        }

        if (Options.EnableByteCandidates)
        {
            AddCandidate(Analysis, MakeByteAbsA(Data, i));
            AddCandidate(Analysis, MakeByteHLImm(Data, i));
        }

        if ((i & 0x1FF) == 0 && ProgressCancelled(Progress))
        {
            OutError = "BuildAnalysis: cancelled";
            return false;
        }
    }

    ProgressSet(Progress, 25, 100);

    // ------------------------------------------------------------
    // 2. Обычные 2 байта через LD HL, Word / LD (nn), HL.
    // ------------------------------------------------------------
    for (int32_t i = 0; i + 1 < ZX_SCREEN_SIZE; ++i)
    {
        if (!RangeIsDirty(Analysis.Dirty, i, i + 2))
        {
            continue;
        }

        if (Options.EnableWordCandidates)
        {
            AddCandidate(Analysis, MakeWordAbsHL(Data, i));
        }

        if ((i & 0x1FF) == 0 && ProgressCancelled(Progress))
        {
            OutError = "BuildAnalysis: cancelled";
            return false;
        }
    }

    ProgressSet(Progress, 40, 100);

    // ------------------------------------------------------------
    // 3. Stack-блоки внутри непрерывных Dirty-участков.
    // ------------------------------------------------------------
    int32_t i = 0;
    while (i < ZX_SCREEN_SIZE)
    {
        while (i < ZX_SCREEN_SIZE && !Analysis.Dirty[i])
        {
            ++i;
        }

        const int32_t RangeStart = i;
        while (i < ZX_SCREEN_SIZE && Analysis.Dirty[i])
        {
            ++i;
        }

        const int32_t RangeEnd = i;
        const int32_t RangeLength = RangeEnd - RangeStart;

        if (RangeLength < 4)
        {
            continue;
        }

        for (int32_t Start = RangeStart; Start < RangeEnd; ++Start)
        {
            const int32_t Available = RangeEnd - Start;
            const int32_t MaxPairs = min(Options.MaxStackPairsToEnumerate, Available / 2);

            // Стек имеет смысл примерно с 2 PUSH, то есть с 4 байт.
            for (int32_t Pairs = 2; Pairs <= MaxPairs; ++Pairs)
            {
                if (Options.EnableStackBlocks)
                {
                    AddCandidate(Analysis, MakeStackBlock(Data, Start, Pairs * 2));
                }

                if ((Pairs & 0x0F) == 0 && ProgressCancelled(Progress))
                {
                    OutError = "BuildAnalysis: cancelled";
                    return false;
                }
            }
        }

        // Большой блок на весь непрерывный участок.
        if (Options.EnableStackBlocks &&
            (RangeLength & 1) == 0 &&
            RangeLength / 2 > Options.MaxStackPairsToEnumerate)
        {
            AddCandidate(Analysis, MakeStackBlock(Data, RangeStart, RangeLength));
        }

        if (ProgressCancelled(Progress))
        {
            OutError = "BuildAnalysis: cancelled";
            return false;
        }
    }

    ProgressSet(Progress, 55, 100);

    // ------------------------------------------------------------
    // 4. Повторяющиеся WORD подряд.
    // ------------------------------------------------------------
    for (int32_t Start = 0; Start + 3 < ZX_SCREEN_SIZE; ++Start)
    {
        if (!RangeIsDirty(Analysis.Dirty, Start, Start + 2))
        {
            continue;
        }

        const uint16_t Word = ReadWordLE(Data, Start);

        int32_t PairCount = 1;
        int32_t Pos = Start + 2;

        while (Pos + 1 < ZX_SCREEN_SIZE)
        {
            if (!RangeIsDirty(Analysis.Dirty, Pos, Pos + 2))
            {
                break;
            }

            if (ReadWordLE(Data, Pos) != Word)
            {
                break;
            }

            ++PairCount;
            Pos += 2;
        }

        if (PairCount >= 2)
        {
            if (Options.EnableRepeatWords)
            {
                AddCandidate(Analysis, MakeRepeatWordAbsHL(Data, Start, PairCount));
                AddCandidate(Analysis, MakeRepeatWordStack(Data, Start, PairCount));
            }
        }

        if ((Start & 0x1FF) == 0 && ProgressCancelled(Progress))
        {
            OutError = "BuildAnalysis: cancelled";
            return false;
        }
    }

    ProgressSet(Progress, 70, 100);

    // ------------------------------------------------------------
    // 5. Горизонтальные серии одинакового байта через INC L.
    //    Нельзя пересекать границу страницы, потому что INC L не меняет H.
    // ------------------------------------------------------------
    for (int32_t Start = 0; Start < ZX_SCREEN_SIZE; ++Start)
    {
        if (!Analysis.Dirty[Start])
        {
            continue;
        }

        const uint16_t StartAddr = AddrOf(Start);
        const uint8_t Value = Data[Start];

        int32_t Length = 1;
        while (Start + Length < ZX_SCREEN_SIZE)
        {
            const uint16_t NextAddr = AddrOf(Start + Length);
            if ((StartAddr & 0xFF00) != (NextAddr & 0xFF00))
            {
                break;
            }

            if (!Analysis.Dirty[Start + Length])
            {
                break;
            }

            if (Data[Start + Length] != Value)
            {
                break;
            }

            ++Length;
        }

        if (Length >= 3)
        {
            if (Options.EnableHorizontalSameByteIncL)
            {
                AddCandidate(Analysis, MakeHorizontalSameByteIncL(Data, Start, Length));
                if (Options.EnableReverseDirections)
                {
                    AddCandidate(Analysis, MakeHorizontalSameByteDecL(Data, Start + Length - 1, Length));
                }
            }

            if (Options.EnableRegisterConstants && Analysis.bHasPreferredBC)
            {
                char RegisterName = 0;
                if (Value == Analysis.PreferredB)
                {
                    RegisterName = 'B';
                }
                else if (Value == Analysis.PreferredC)
                {
                    RegisterName = 'C';
                }

                if (RegisterName != 0)
                {
                    AddCandidate(Analysis, MakeHorizontalSameByteRegIncL(Data, Start, Length, RegisterName));
                    if (Options.EnableReverseDirections)
                    {
                        AddCandidate(Analysis, MakeHorizontalSameByteRegDecL(Data, Start + Length - 1, Length, RegisterName));
                    }
                }
            }
        }

        if ((Start & 0x3FF) == 0 && ProgressCancelled(Progress))
        {
            OutError = "BuildAnalysis: cancelled";
            return false;
        }
    }

    ProgressSet(Progress, 85, 100);

    // ------------------------------------------------------------
    // 6. Вертикальные серии через INC H и повторяющиеся WORD с шагом #0100.
    //    Это отражает геометрию экрана ZX: соседние raster-строки часто лежат
    //    в адресах addr, addr+#0100, addr+#0200...
    // ------------------------------------------------------------
    if (Options.EnableVerticalCandidates)
    {
        for (int32_t Start = 0; Start < ZX_SCREEN_SIZE; ++Start)
        {
            if (!Analysis.Dirty[Start])
            {
                continue;
            }

            int32_t Count = 1;
            while (Start + Count * 0x0100 < ZX_SCREEN_SIZE && Analysis.Dirty[Start + Count * 0x0100])
            {
                ++Count;
            }

            if (Count >= 3)
            {
                AddCandidate(Analysis, MakeVerticalBytesIncH(Data, Start, Count));
                if (Options.EnableReverseDirections)
                {
                    AddCandidate(Analysis, MakeVerticalBytesDecH(Data, Start + (Count - 1) * 0x0100, Count));
                }

                const uint8_t Value = Data[Start];
                int32_t SameCount = 1;
                while (SameCount < Count && Data[Start + SameCount * 0x0100] == Value)
                {
                    ++SameCount;
                }

                if (SameCount >= 3)
                {
                    AddCandidate(Analysis, MakeVerticalSameByteIncH(Data, Start, SameCount));
                    if (Options.EnableReverseDirections)
                    {
                        AddCandidate(Analysis, MakeVerticalSameByteDecH(Data, Start + (SameCount - 1) * 0x0100, SameCount));
                    }

                    if (Options.EnableRegisterConstants && Analysis.bHasPreferredBC)
                    {
                        char RegisterName = 0;
                        if (Value == Analysis.PreferredB)
                        {
                            RegisterName = 'B';
                        }
                        else if (Value == Analysis.PreferredC)
                        {
                            RegisterName = 'C';
                        }

                        if (RegisterName != 0)
                        {
                            AddCandidate(Analysis, MakeVerticalSameByteRegIncH(Data, Start, SameCount, RegisterName));
                            if (Options.EnableReverseDirections)
                            {
                                AddCandidate(Analysis, MakeVerticalSameByteRegDecH(Data, Start + (SameCount - 1) * 0x0100, SameCount, RegisterName));
                            }
                        }
                    }
                }
            }

            if (Start + 1 < ZX_SCREEN_SIZE && RangeIsDirty(Analysis.Dirty, Start, Start + 2))
            {
                const uint16_t Word = ReadWordLE(Data, Start);
                int32_t WordCount = 1;
                while (Start + WordCount * 0x0100 + 1 < ZX_SCREEN_SIZE &&
                    RangeIsDirty(Analysis.Dirty, Start + WordCount * 0x0100, Start + WordCount * 0x0100 + 2) &&
                    ReadWordLE(Data, Start + WordCount * 0x0100) == Word)
                {
                    ++WordCount;
                }

                if (WordCount >= 2)
                {
                    AddCandidate(Analysis, MakeVerticalRepeatWordAbsHL(Data, Start, WordCount));
                }
            }

            if ((Start & 0x3FF) == 0 && ProgressCancelled(Progress))
            {
                OutError = "BuildAnalysis: cancelled";
                return false;
            }
        }
    }

    ProgressSet(Progress, 100, 100);
    OutAnalysis = std::move(Analysis);
    return true;
}

namespace
{
    int32_t CountDirtyBytes(const std::vector<uint8_t>& Dirty)
    {
        int32_t Count = 0;
        for (uint8_t Value : Dirty)
        {
            if (Value)
            {
                ++Count;
            }
        }
        return Count;
    }

    int32_t MarkCandidateClean(std::vector<uint8_t>& Dirty, const CodeGenerator::FCandidate& Candidate)
    {
        int32_t Cleaned = 0;
        if (Candidate.Linear)
        {
            for (int32_t i = Candidate.StartOffset; i < Candidate.EndOffset; ++i)
            {
                if (Dirty[i])
                {
                    Dirty[i] = 0;
                    ++Cleaned;
                }
            }
            return Cleaned;
        }

        for (int32_t Offset : Candidate.CoveredOffsets)
        {
            if (Dirty[Offset])
            {
                Dirty[Offset] = 0;
                ++Cleaned;
            }
        }

        return Cleaned;
    }

    bool CandidateOverlaps(const CodeGenerator::FCandidate& A, const CodeGenerator::FCandidate& B)
    {
        if (A.Linear && B.Linear)
        {
            return A.StartOffset < B.EndOffset && B.StartOffset < A.EndOffset;
        }

        const CodeGenerator::FCandidate& Sparse = A.Linear ? B : A;
        const CodeGenerator::FCandidate& Other = A.Linear ? A : B;
        for (int32_t Offset : Sparse.CoveredOffsets)
        {
            if (CodeGenerator::CandidateCoversOffset(Other, Offset))
            {
                return true;
            }
        }

        return false;
    }

    int64_t ScorePlan(int32_t Cycles, int32_t CodeBytes, const CodeGenerator::FOptions& Options)
    {
        return static_cast<int64_t>(Cycles) * Options.CycleWeight + static_cast<int64_t>(CodeBytes) * Options.ByteWeight;
    }

    bool CandidateIdsUseBC(const CodeGenerator::FAnalysis& Analysis, const std::vector<int32_t>& CandidateIds)
    {
        for (int32_t ID : CandidateIds)
        {
            if (Analysis.Candidates[ID].RequiresBC)
            {
                return true;
            }
        }
        return false;
    }

    bool CandidateUsesStack(const CodeGenerator::FCandidate& Candidate)
    {
        return Candidate.Kind == CodeGenerator::EOpKind::StackBlock ||
            Candidate.Kind == CodeGenerator::EOpKind::RepeatWordStack;
    }

    bool CandidateIdsUseStack(const CodeGenerator::FAnalysis& Analysis, const std::vector<int32_t>& CandidateIds)
    {
        for (int32_t ID : CandidateIds)
        {
            if (CandidateUsesStack(Analysis.Candidates[ID]))
            {
                return true;
            }
        }
        return false;
    }

    void AddStackOverheadIfNeeded(int32_t& Cycles, int32_t& CodeBytes, const CodeGenerator::FOptions& Options)
    {
        if (Options.DisableInterruptsForStack)
        {
            Cycles += 8;
            CodeBytes += 2;
        }

        if (Options.PreserveSP)
        {
            Cycles += 132;
            CodeBytes += 25;
        }
    }

    int64_t ScorePlanWithBC(
        int32_t Cycles,
        int32_t CodeBytes,
        bool bNeedsBC,
        bool bBCAlreadyLoaded,
        const CodeGenerator::FOptions& Options)
    {
        if (bNeedsBC && !bBCAlreadyLoaded)
        {
            // LD BC,nn
            Cycles += 10;
            CodeBytes += 3;
        }

        return ScorePlan(Cycles, CodeBytes, Options);
    }

    bool BuildLinearPlan(
        const CodeGenerator::FAnalysis& Analysis,
        const CodeGenerator::FOptions& Options,
        const std::vector<uint8_t>& Dirty,
        std::vector<int32_t>& OutCandidateIds,
        int32_t& OutCycles,
        int32_t& OutCodeBytes,
        std::string& OutError)
    {
        const int32_t N = CodeGenerator::ZX_SCREEN_SIZE;
        const int64_t INF = (std::numeric_limits<int64_t>::max)() / 4;

        std::vector<int64_t> DP(N + 1, INF);
        std::vector<int32_t> Choice(N + 1, -1);

        DP[N] = 0;

        for (int32_t i = N - 1; i >= 0; --i)
        {
            if (!Dirty[i])
            {
                DP[i] = DP[i + 1];
                Choice[i] = -2;
                continue;
            }

            for (int32_t ID : Analysis.StartsAt[i])
            {
                const CodeGenerator::FCandidate& Candidate = Analysis.Candidates[ID];
                if (!Candidate.Linear)
                {
                    continue;
                }

                if (Candidate.EndOffset <= i || Candidate.EndOffset > N)
                {
                    continue;
                }

                if (!CodeGenerator::CandidateIsDirty(Dirty, Candidate))
                {
                    continue;
                }

                if (DP[Candidate.EndOffset] == INF)
                {
                    continue;
                }

                const int64_t Cost = CodeGenerator::ScoreCandidate(Candidate, Options) + DP[Candidate.EndOffset];
                if (Cost < DP[i])
                {
                    DP[i] = Cost;
                    Choice[i] = ID;
                }
            }
        }

        OutCandidateIds.clear();
        OutCycles = 0;
        OutCodeBytes = 0;

        int32_t Pos = 0;
        while (Pos < N)
        {
            if (!Dirty[Pos])
            {
                ++Pos;
                continue;
            }

            const int32_t ID = Choice[Pos];
            if (ID < 0)
            {
                OutError = "OptimizePlan: failed to cover dirty byte";
                return false;
            }

            const CodeGenerator::FCandidate& Candidate = Analysis.Candidates[ID];
            OutCandidateIds.push_back(ID);
            OutCycles += Candidate.Cycles;
            OutCodeBytes += Candidate.CodeBytes;
            Pos = Candidate.EndOffset;
        }

        return true;
    }
}

bool CodeGenerator::OptimizePlan(const FAnalysis& Analysis, const FOptions& Options, FPlan& OutPlan, std::string& OutError, const FProgressInfo* Progress)
{
    OutPlan = FPlan();

    struct FSearchState
    {
        std::vector<uint8_t> Remaining;
        std::vector<int32_t> CandidateIds;
        int32_t RemainingCount = 0;
        int32_t Cycles = 0;
        int32_t CodeBytes = 0;
        bool UsesBC = false;
        bool UsesStack = false;
    };

    auto ScoreState = [&Options](const FSearchState& State)
    {
        return ScorePlan(State.Cycles, State.CodeBytes, Options);
    };

    const int32_t BeamWidth = max(1, Options.NonLinearBeamWidth);
    const int32_t MaxCandidatesToEvaluatePerPass = max(1, Options.MaxNonLinearCandidatesToEvaluatePerPass);
    const int32_t InitialDirty = CountDirtyBytes(Analysis.Dirty);

    std::vector<FSearchState> Beam;
    Beam.push_back(FSearchState{});
    Beam.back().Remaining = Analysis.Dirty;
    Beam.back().RemainingCount = InitialDirty;

    bool bHasBest = false;
    FSearchState BestFinished;
    int64_t BestFinishedScore = (std::numeric_limits<int64_t>::max)() / 4;

    ProgressSet(Progress, 0, 100);
    while (!Beam.empty())
    {
        if (ProgressCancelled(Progress))
        {
            OutError = "OptimizePlan: cancelled";
            return false;
        }

        std::vector<FSearchState> NextBeam;
        int32_t BestBeamRemaining = InitialDirty;

        for (const FSearchState& State : Beam)
        {
            BestBeamRemaining = min(BestBeamRemaining, State.RemainingCount);

            if (State.RemainingCount == 0)
            {
                const int64_t StateScore = ScoreState(State);
                if (!bHasBest || StateScore < BestFinishedScore)
                {
                    BestFinished = State;
                    BestFinishedScore = StateScore;
                    bHasBest = true;
                }
                continue;
            }

            std::vector<int32_t> LinearIds;
            int32_t LinearCycles = 0;
            int32_t LinearCodeBytes = 0;
            if (!BuildLinearPlan(Analysis, Options, State.Remaining, LinearIds, LinearCycles, LinearCodeBytes, OutError))
            {
                return false;
            }

            const bool bLinearNeedsBC = CandidateIdsUseBC(Analysis, LinearIds);
            const bool bLinearNeedsStack = CandidateIdsUseStack(Analysis, LinearIds);

            FSearchState Finished = State;
            Finished.CandidateIds.insert(Finished.CandidateIds.end(), LinearIds.begin(), LinearIds.end());
            Finished.Cycles += LinearCycles;
            Finished.CodeBytes += LinearCodeBytes;
            Finished.RemainingCount = 0;
            if (!Finished.UsesBC && bLinearNeedsBC)
            {
                Finished.Cycles += 10;
                Finished.CodeBytes += 3;
                Finished.UsesBC = true;
            }
            if (!Finished.UsesStack && bLinearNeedsStack)
            {
                AddStackOverheadIfNeeded(Finished.Cycles, Finished.CodeBytes, Options);
                Finished.UsesStack = true;
            }

            const int64_t FinishedScore = ScoreState(Finished);
            if (!bHasBest || FinishedScore < BestFinishedScore)
            {
                BestFinished = Finished;
                BestFinishedScore = FinishedScore;
                bHasBest = true;
            }

            struct FNonLinearProbe
            {
                int32_t ID;
                int64_t ApproxSaving;
            };

            std::vector<FNonLinearProbe> Probes;
            Probes.reserve(MaxCandidatesToEvaluatePerPass);

            for (int32_t ID = 0; ID < static_cast<int32_t>(Analysis.Candidates.size()); ++ID)
            {
                if (ProgressCancelled(Progress))
                {
                    OutError = "OptimizePlan: cancelled";
                    return false;
                }

                const FCandidate& Candidate = Analysis.Candidates[ID];
                if (Candidate.Linear || !CandidateIsDirty(State.Remaining, Candidate))
                {
                    continue;
                }

                int64_t OverlappedLinearCost = 0;
                for (int32_t LinearID : LinearIds)
                {
                    const FCandidate& LinearCandidate = Analysis.Candidates[LinearID];
                    if (CandidateOverlaps(Candidate, LinearCandidate))
                    {
                        OverlappedLinearCost += ScoreCandidate(LinearCandidate, Options);
                    }
                }

                const int64_t CandidateCost = ScoreCandidate(Candidate, Options);
                if (CandidateCost >= OverlappedLinearCost)
                {
                    continue;
                }

                Probes.push_back({ ID, OverlappedLinearCost - CandidateCost });
            }

            std::sort(Probes.begin(), Probes.end(),
                [](const FNonLinearProbe& A, const FNonLinearProbe& B)
                {
                    return A.ApproxSaving > B.ApproxSaving;
                });

            const int32_t ProbeCount = min(MaxCandidatesToEvaluatePerPass, static_cast<int32_t>(Probes.size()));
            for (int32_t ProbeIndex = 0; ProbeIndex < ProbeCount; ++ProbeIndex)
            {
                if (ProgressCancelled(Progress))
                {
                    OutError = "OptimizePlan: cancelled";
                    return false;
                }

                const FCandidate& Candidate = Analysis.Candidates[Probes[ProbeIndex].ID];

                FSearchState Child = State;
                Child.CandidateIds.push_back(Probes[ProbeIndex].ID);
                Child.Cycles += Candidate.Cycles;
                Child.CodeBytes += Candidate.CodeBytes;

                if (!Child.UsesBC && Candidate.RequiresBC)
                {
                    Child.Cycles += 10;
                    Child.CodeBytes += 3;
                    Child.UsesBC = true;
                }
                if (!Child.UsesStack && CandidateUsesStack(Candidate))
                {
                    AddStackOverheadIfNeeded(Child.Cycles, Child.CodeBytes, Options);
                    Child.UsesStack = true;
                }

                const int32_t Cleaned = MarkCandidateClean(Child.Remaining, Candidate);
                Child.RemainingCount = max(0, Child.RemainingCount - Cleaned);
                NextBeam.push_back(std::move(Child));
            }
        }

        std::sort(NextBeam.begin(), NextBeam.end(),
            [&ScoreState](const FSearchState& A, const FSearchState& B)
            {
                return ScoreState(A) < ScoreState(B);
            });

        if (static_cast<int32_t>(NextBeam.size()) > BeamWidth)
        {
            NextBeam.resize(BeamWidth);
        }

        if (!NextBeam.empty())
        {
            int32_t BestRemaining = InitialDirty;
            for (const FSearchState& State : NextBeam)
            {
                BestRemaining = min(BestRemaining, State.RemainingCount);
            }
            if (InitialDirty > 0)
            {
                const int32_t Covered = InitialDirty - BestRemaining;
                ProgressSet(Progress, Covered, InitialDirty);
            }
        }

        Beam = std::move(NextBeam);
    }

    if (!bHasBest)
    {
        OutError = "OptimizePlan: failed to produce a plan";
        return false;
    }

    OutPlan.CandidateIds = std::move(BestFinished.CandidateIds);
    OutPlan.TotalCycles = BestFinished.Cycles;
    OutPlan.TotalCodeBytes = BestFinished.CodeBytes;
    OutPlan.UsesBC = BestFinished.UsesBC;
    OutPlan.UsesStack = BestFinished.UsesStack;
    ProgressSet(Progress, InitialDirty, InitialDirty);
    return true;
}

void CodeGenerator::EmitByte(FEmitOutput& Out, uint8_t Value)
{
    Out.Code.push_back(Value);
}

void CodeGenerator::EmitWordLE(FEmitOutput& Out, uint16_t Value)
{
    EmitByte(Out, static_cast<uint8_t>(Value & 0x00FF));
    EmitByte(Out, static_cast<uint8_t>((Value >> 8) & 0x00FF));
}

void CodeGenerator::PatchWordLE(FEmitOutput& Out, size_t Offset, uint16_t Value)
{
    if (Offset + 1 >= Out.Code.size())
    {
        return;
    }

    Out.Code[Offset] = static_cast<uint8_t>(Value & 0x00FF);
    Out.Code[Offset + 1] = static_cast<uint8_t>((Value >> 8) & 0x00FF);
}

void CodeGenerator::EmitLdA(FEmitOutput& Out, FEmitState& State, uint8_t Value)
{
    if (!State.bHasA || State.A != Value)
    {
        Out.Preview << "                LD A, " << Hex8(Value) << "\n";
        EmitByte(Out, 0x3E);
        EmitByte(Out, Value);
        Out.Cycles += 7;
        State.A = Value;
        State.bHasA = true;
    }
}

void CodeGenerator::EmitLdHL(FEmitOutput& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasHL || State.HL != Value)
    {
        Out.Preview << "                LD HL, " << Hex16(Value) << "\n";
        EmitByte(Out, 0x21);
        EmitWordLE(Out, Value);
        Out.Cycles += 10;
        State.HL = Value;
        State.bHasHL = true;
    }
}

void CodeGenerator::EmitLdSP(FEmitOutput& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasSP || State.SP != Value)
    {
        Out.Preview << "                LD SP, " << Hex16(Value) << "\n";
        EmitByte(Out, 0x31);
        EmitWordLE(Out, Value);
        Out.Cycles += 10;
        State.SP = Value;
        State.bHasSP = true;
    }
}

bool CodeGenerator::EmitCandidate(FEmitOutput& Out, const FCandidate& Candidate, const std::vector<uint8_t>& Data, FEmitState& State, std::string& OutError)
{
    switch (Candidate.Kind)
    {
    case EOpKind::ByteAbsA:
    {
        EmitLdA(Out, State, Candidate.ByteValue);
        Out.Preview << "                LD (" << Hex16(Candidate.StartAddr) << "), A\n";
        EmitByte(Out, 0x32);
        EmitWordLE(Out, Candidate.StartAddr);
        Out.Cycles += 13;
        break;
    }

    case EOpKind::ByteHLImm:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);
        Out.Preview << "                LD (HL), " << Hex8(Candidate.ByteValue) << "\n";
        EmitByte(Out, 0x36);
        EmitByte(Out, Candidate.ByteValue);
        Out.Cycles += 10;

        // HL не меняется.
        break;
    }

    case EOpKind::WordAbsHL:
    {
        EmitLdHL(Out, State, Candidate.WordValue);
        Out.Preview << "                LD (" << Hex16(Candidate.StartAddr) << "), HL\n";
        EmitByte(Out, 0x22);
        EmitWordLE(Out, Candidate.StartAddr);
        Out.Cycles += 16;
        break;
    }

    case EOpKind::StackBlock:
    {
        const uint16_t endAddr = AddrOf(Candidate.EndOffset);
        EmitLdSP(Out, State, endAddr);

        for (int32_t Pos = Candidate.EndOffset - 2; Pos >= Candidate.StartOffset; Pos -= 2)
        {
            const uint16_t Word = ReadWordLE(Data, Pos);
            EmitLdHL(Out, State, Word);
            Out.Preview << "                PUSH HL\n";
            EmitByte(Out, 0xE5);
            Out.Cycles += 11;

            State.SP = static_cast<uint16_t>(State.SP - 2);
            State.bHasSP = true;
        }

        break;
    }

    case EOpKind::RepeatWordAbsHL:
    {
        EmitLdHL(Out, State, Candidate.WordValue);

        for (int32_t Pos = Candidate.StartOffset; Pos < Candidate.EndOffset; Pos += 2)
        {
            Out.Preview << "                LD (" << Hex16(AddrOf(Pos)) << "), HL\n";
            EmitByte(Out, 0x22);
            EmitWordLE(Out, AddrOf(Pos));
            Out.Cycles += 16;
        }

        break;
    }

    case EOpKind::RepeatWordStack:
    {
        const uint16_t EndAddr = AddrOf(Candidate.EndOffset);
        EmitLdSP(Out, State, EndAddr);
        EmitLdHL(Out, State, Candidate.WordValue);

        const int32_t PairCount = Candidate.WriteBytes / 2;
        for (int32_t i = 0; i < PairCount; ++i)
        {
            Out.Preview << "                PUSH HL\n";
            EmitByte(Out, 0xE5);
            Out.Cycles += 11;

            State.SP = static_cast<uint16_t>(State.SP - 2);
            State.bHasSP = true;
        }

        break;
    }

    case EOpKind::HorizontalSameByteIncL:
    case EOpKind::HorizontalSameByteDecL:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);
        EmitLdA(Out, State, Candidate.ByteValue);

        const bool bDec = Candidate.Kind == EOpKind::HorizontalSameByteDecL;
        const uint8_t StepOpcode = bDec ? 0x2D : 0x2C;
        const char* StepText = bDec ? "DEC L" : "INC L";

        Out.Preview << "                LD (HL), A\n";
        EmitByte(Out, 0x77);
        Out.Cycles += 7;

        uint16_t HL = Candidate.StartAddr;
        for (int32_t i = 1; i < Candidate.WriteBytes; ++i)
        {
            Out.Preview << "                " << StepText << "\n";
            EmitByte(Out, StepOpcode);
            Out.Cycles += 4;
            Out.Preview << "                LD (HL), A\n";
            EmitByte(Out, 0x77);
            Out.Cycles += 7;
            HL = static_cast<uint16_t>((HL & 0xFF00) | ((HL + Candidate.Stride) & 0x00FF));
        }

        State.HL = HL;
        State.bHasHL = true;
        break;
    }

    case EOpKind::HorizontalSameByteRegIncL:
    case EOpKind::HorizontalSameByteRegDecL:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);

        const uint8_t StoreOpcode = Candidate.RegisterName == 'B' ? 0x70 : 0x71;
        const uint8_t StepOpcode = Candidate.Kind == EOpKind::HorizontalSameByteRegIncL ? 0x2C : 0x2D;
        const char* StepText = Candidate.Kind == EOpKind::HorizontalSameByteRegIncL ? "INC L" : "DEC L";

        uint16_t HL = Candidate.StartAddr;
        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            Out.Preview << "                LD (HL), " << Candidate.RegisterName << "\n";
            EmitByte(Out, StoreOpcode);
            Out.Cycles += 7;

            if (i + 1 < Candidate.Count)
            {
                Out.Preview << "                " << StepText << "\n";
                EmitByte(Out, StepOpcode);
                Out.Cycles += 4;
                HL = static_cast<uint16_t>((HL & 0xFF00) | ((HL + Candidate.Stride) & 0x00FF));
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        break;
    }

    case EOpKind::VerticalBytesIncH:
    case EOpKind::VerticalBytesDecH:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);

        const bool bDec = Candidate.Kind == EOpKind::VerticalBytesDecH;
        const uint8_t StepOpcode = bDec ? 0x25 : 0x24;
        const char* StepText = bDec ? "DEC H" : "INC H";
        uint16_t HL = Candidate.StartAddr;
        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            const int32_t Offset = Candidate.CoveredOffsets[i];
            Out.Preview << "                LD (HL), " << Hex8(Data[Offset]) << "\n";
            EmitByte(Out, 0x36);
            EmitByte(Out, Data[Offset]);
            Out.Cycles += 10;

            if (i + 1 < Candidate.Count)
            {
                Out.Preview << "                " << StepText << "\n";
                EmitByte(Out, StepOpcode);
                Out.Cycles += 4;
                HL = static_cast<uint16_t>(HL + Candidate.Stride);
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        break;
    }

    case EOpKind::VerticalSameByteIncH:
    case EOpKind::VerticalSameByteDecH:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);
        EmitLdA(Out, State, Candidate.ByteValue);

        const bool bDec = Candidate.Kind == EOpKind::VerticalSameByteDecH;
        const uint8_t StepOpcode = bDec ? 0x25 : 0x24;
        const char* StepText = bDec ? "DEC H" : "INC H";
        uint16_t HL = Candidate.StartAddr;
        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            Out.Preview << "                LD (HL), A\n";
            EmitByte(Out, 0x77);
            Out.Cycles += 7;

            if (i + 1 < Candidate.Count)
            {
                Out.Preview << "                " << StepText << "\n";
                EmitByte(Out, StepOpcode);
                Out.Cycles += 4;
                HL = static_cast<uint16_t>(HL + Candidate.Stride);
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        break;
    }

    case EOpKind::VerticalSameByteRegIncH:
    case EOpKind::VerticalSameByteRegDecH:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);

        const bool bDec = Candidate.Kind == EOpKind::VerticalSameByteRegDecH;
        const uint8_t StoreOpcode = Candidate.RegisterName == 'B' ? 0x70 : 0x71;
        const uint8_t StepOpcode = bDec ? 0x25 : 0x24;
        const char* StepText = bDec ? "DEC H" : "INC H";

        uint16_t HL = Candidate.StartAddr;
        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            Out.Preview << "                LD (HL), " << Candidate.RegisterName << "\n";
            EmitByte(Out, StoreOpcode);
            Out.Cycles += 7;

            if (i + 1 < Candidate.Count)
            {
                Out.Preview << "                " << StepText << "\n";
                EmitByte(Out, StepOpcode);
                Out.Cycles += 4;
                HL = static_cast<uint16_t>(HL + Candidate.Stride);
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        break;
    }

    case EOpKind::VerticalRepeatWordAbsHL:
    {
        EmitLdHL(Out, State, Candidate.WordValue);

        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            const int32_t Offset = Candidate.StartOffset + i * Candidate.Stride;
            Out.Preview << "                LD (" << Hex16(AddrOf(Offset)) << "), HL\n";
            EmitByte(Out, 0x22);
            EmitWordLE(Out, AddrOf(Offset));
            Out.Cycles += 16;
        }

        break;
    }

    default:
        OutError = "EmitCandidate: unknown candidate kind";
        return false;
    }

    return true;
}

bool CodeGenerator::EmitAsm(const FAnalysis& Analysis, const FPlan& Plan, const FOptions& Options, std::string& OutAsm, std::vector<uint8_t>& OutCode, int32_t& OutCycles, std::string& OutError, const std::string& LabelName, const FProgressInfo* Progress)
{
    FEmitOutput Out;
    FEmitState State;
    size_t RestoreSPOperandOffset = (std::numeric_limits<size_t>::max)();
    size_t CurrentAddressOffset = (std::numeric_limits<size_t>::max)();
    uint16_t RestoreSPRelativeOffset = 0;
    bool bHasRestoreSPRelativeOffset = false;
    size_t RestoreSPPreviewOffset = (std::numeric_limits<size_t>::max)();
    static constexpr const char* RestoreSPPreviewPlaceholder = "                LD BC, #0000\n";
    const bool bPlanUsesStack = Plan.UsesStack;
    const bool bPreserveSP = Options.PreserveSP && bPlanUsesStack;

    ProgressSet(Progress, 0, 100);
    if (ProgressCancelled(Progress))
    {
        OutError = "EmitAsm: cancelled";
        return false;
    }

    Out.Preview << LabelName << ":\n";
    if (Options.DisableInterruptsForStack && bPlanUsesStack)
    {
        Out.Preview << "                DI\n";
        EmitByte(Out, 0xF3);
        Out.Cycles += 4;
    }

    if (bPreserveSP)
    {
        // Сохраняем исходный SP в DE, затем получаем текущий адрес без знания
        // фактического адреса размещения кода.
        Out.Preview << "                LD HL, #0000\n";
        EmitByte(Out, 0x21);
        EmitWordLE(Out, 0x0000);
        Out.Cycles += 10;

        Out.Preview << "                ADD HL, SP\n";
        EmitByte(Out, 0x39);
        Out.Cycles += 11;

        Out.Preview << "                EX DE, HL\n";
        EmitByte(Out, 0xEB);
        Out.Cycles += 4;

        Out.Preview << "                LD SP, " << Hex16(Options.StackTopAddress) << "\n";
        EmitByte(Out, 0x31);
        EmitWordLE(Out, Options.StackTopAddress);
        Out.Cycles += 10;

        Out.Preview << "                LD HL, #E9E1\n";
        EmitByte(Out, 0x21);
        EmitWordLE(Out, 0xE9E1);
        Out.Cycles += 10;

        Out.Preview << "                EX (SP), HL\n";
        EmitByte(Out, 0xE3);
        Out.Cycles += 19;

        Out.Preview << "                CALL " << Hex16(Options.StackTopAddress) << "\n";
        EmitByte(Out, 0xCD);
        EmitWordLE(Out, Options.StackTopAddress);
        Out.Cycles += 17/* CALL */ + 10/* POP HL */ + 4/* JP (HL) */;

        CurrentAddressOffset = Out.Code.size();
        RestoreSPPreviewOffset = static_cast<size_t>(Out.Preview.tellp());
        Out.Preview << RestoreSPPreviewPlaceholder;
        EmitByte(Out, 0x01);
        RestoreSPOperandOffset = Out.Code.size();
        EmitWordLE(Out, 0x0000);
        Out.Cycles += 10;

        Out.Preview << "                ADD HL, BC\n";
        EmitByte(Out, 0x09);
        Out.Cycles += 11;

        Out.Preview << "                LD (HL), E\n";
        EmitByte(Out, 0x73);
        Out.Cycles += 7;

        Out.Preview << "                INC HL\n";
        EmitByte(Out, 0x23);
        Out.Cycles += 6;

        Out.Preview << "                LD (HL), D\n";
        EmitByte(Out, 0x72);
        Out.Cycles += 7;

        State.bHasHL = false;
        State.bHasSP = true;
        State.SP = Options.StackTopAddress;
    }
    if (Plan.UsesBC && Analysis.bHasPreferredBC)
    {
        const uint16_t BC = static_cast<uint16_t>((Analysis.PreferredB << 8) | Analysis.PreferredC);
        Out.Preview << "                LD BC, " << Hex16(BC) << "\n";
        EmitByte(Out, 0x01);
        EmitWordLE(Out, BC);
        Out.Cycles += 10;
    }
    Out.Preview << "\n";

    const int32_t CandidateCount = static_cast<int32_t>(Plan.CandidateIds.size());
    for (int32_t Index = 0; Index < CandidateCount; ++Index)
    {
        if (ProgressCancelled(Progress))
        {
            OutError = "EmitAsm: cancelled";
            return false;
        }

        const int32_t ID = Plan.CandidateIds[Index];
        const FCandidate& Candidate = Analysis.Candidates[ID];
        Out.Preview << "                ; "
            << ToString(Candidate.Kind)
            << " addr=" << Hex16(Candidate.StartAddr)
            << " bytes=" << Candidate.WriteBytes
            << " cycles=" << Candidate.Cycles
            << " code=" << Candidate.CodeBytes
            << "\n";

        if (!EmitCandidate(Out, Candidate, Analysis.Data, State, OutError))
        {
            return false;
        }
        Out.Preview << "\n";

        if (CandidateCount > 0)
        {
            ProgressSet(Progress, 5 + ((Index + 1) * 85) / CandidateCount, 100);
        }
    }

    if (bPreserveSP)
    {
        Out.Preview << "._RestoreSP:\n";
        Out.Preview << "                LD SP, #0000\n";
        EmitByte(Out, 0x31);
        const size_t RestoreSPValueOffset = Out.Code.size();
        EmitWordLE(Out, 0x0000);
        Out.Cycles += 10;

        if (RestoreSPOperandOffset != (std::numeric_limits<size_t>::max)() &&
            CurrentAddressOffset != (std::numeric_limits<size_t>::max)())
        {
            const uint16_t RelativeOffset = static_cast<uint16_t>(RestoreSPValueOffset - CurrentAddressOffset);
            PatchWordLE(Out, RestoreSPOperandOffset, RelativeOffset);
            RestoreSPRelativeOffset = RelativeOffset;
            bHasRestoreSPRelativeOffset = true;
        }
    }

    if (Options.DisableInterruptsForStack && bPlanUsesStack)
    {
        Out.Preview << "                EI\n";
        EmitByte(Out, 0xFB);
        Out.Cycles += 4;
    }

    Out.Preview << "                RET\n";
    EmitByte(Out, 0xC9);
    Out.Cycles += 10;

    OutAsm = Out.Preview.str();
    if (bHasRestoreSPRelativeOffset && RestoreSPPreviewOffset != (std::numeric_limits<size_t>::max)())
    {
        OutAsm.replace(RestoreSPPreviewOffset, std::strlen(RestoreSPPreviewPlaceholder), "                LD BC, " + Hex16(RestoreSPRelativeOffset) + "\n");
    }
    OutCode = std::move(Out.Code);
    OutCycles = Out.Cycles;
    if (OutAsm.empty())
    {
        OutError = "EmitAsm: produced empty output";
        return false;
    }
    if (OutCode.empty())
    {
        OutError = "EmitAsm: produced empty bytecode";
        return false;
    }

    ProgressSet(Progress, 100, 100);
    return true;
}

void CodeGenerator::PrintPlanSummary(const FAnalysis& Analysis, const FPlan& Plan)
{
    std::cout << "Plan operations: " << Plan.CandidateIds.size() << "\n";
    std::cout << "Estimated cycles: " << Plan.TotalCycles << "\n";
    std::cout << "Estimated code bytes: " << Plan.TotalCodeBytes << "\n";

    int32_t StackCount = 0;
    int32_t WordCount = 0;
    int32_t ByteCount = 0;
    int32_t RepeatCount = 0;
    int32_t HorizCount = 0;
    int32_t VertCount = 0;
    int32_t RegisterCount = 0;

    for (int32_t ID : Plan.CandidateIds)
    {
        const FCandidate& Candidate = Analysis.Candidates[ID];

        switch (Candidate.Kind)
        {
        case EOpKind::StackBlock:
            ++StackCount;
            break;

        case EOpKind::WordAbsHL:
            ++WordCount;
            break;

        case EOpKind::ByteAbsA:
        case EOpKind::ByteHLImm:
            ++ByteCount;
            break;

        case EOpKind::RepeatWordAbsHL:
        case EOpKind::RepeatWordStack:
            ++RepeatCount;
            break;

        case EOpKind::HorizontalSameByteIncL:
        case EOpKind::HorizontalSameByteDecL:
        case EOpKind::HorizontalSameByteRegIncL:
        case EOpKind::HorizontalSameByteRegDecL:
            ++HorizCount;
            break;

        case EOpKind::VerticalBytesIncH:
        case EOpKind::VerticalBytesDecH:
        case EOpKind::VerticalSameByteIncH:
        case EOpKind::VerticalSameByteDecH:
        case EOpKind::VerticalSameByteRegIncH:
        case EOpKind::VerticalSameByteRegDecH:
        case EOpKind::VerticalRepeatWordAbsHL:
            ++VertCount;
            break;
        }

        if (Candidate.RequiresBC)
        {
            ++RegisterCount;
        }
    }

    std::cout << "Stack blocks: " << StackCount << "\n";
    std::cout << "Word writes: " << WordCount << "\n";
    std::cout << "Byte writes: " << ByteCount << "\n";
    std::cout << "Repeat blocks: " << RepeatCount << "\n";
    std::cout << "Horizontal runs: " << HorizCount << "\n";
    std::cout << "Vertical runs: " << VertCount << "\n";
    std::cout << "Register constant runs: " << RegisterCount << "\n";
}
