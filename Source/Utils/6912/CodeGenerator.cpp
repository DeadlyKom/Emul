#include "CodeGenerator.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace
{
    enum class EStackPairLoadKind
    {
        None,
        Full,
        High,
        Low,
        Bytes,
    };

    struct FStackPairValue
    {
        bool bHasValue;
        uint16_t Value;
    };

    struct FStackPairLoadChoice
    {
        int32_t PairIndex;
        EStackPairLoadKind LoadKind;
        bool bHighFromRegister;
        bool bLowFromRegister;
        char HighRegisterName;
        char LowRegisterName;
        int32_t Cycles;
        int32_t CodeBytes;
    };

    static const char* StackPairName(int32_t PairIndex)
    {
        switch (PairIndex)
        {
        case 0: return "HL";
        case 1: return "DE";
        case 2: return "BC";
        default: return "HL";
        }
    }

    static uint8_t WordLow(uint16_t Value)
    {
        return static_cast<uint8_t>(Value & 0x00FF);
    }

    static uint8_t WordHigh(uint16_t Value)
    {
        return static_cast<uint8_t>((Value >> 8) & 0x00FF);
    }

    static char StackPairByteRegisterName(int32_t PairIndex, bool bHigh)
    {
        switch (PairIndex)
        {
        case 0: return bHigh ? 'H' : 'L';
        case 1: return bHigh ? 'D' : 'E';
        case 2: return bHigh ? 'B' : 'C';
        default: return bHigh ? 'H' : 'L';
        }
    }

    static char FindKnownByteRegister(const FStackPairValue* Pairs, uint8_t Value)
    {
        for (int32_t PairIndex = 0; PairIndex < 3; ++PairIndex)
        {
            if (!Pairs[PairIndex].bHasValue)
            {
                continue;
            }

            if (WordHigh(Pairs[PairIndex].Value) == Value)
            {
                return StackPairByteRegisterName(PairIndex, true);
            }
            if (WordLow(Pairs[PairIndex].Value) == Value)
            {
                return StackPairByteRegisterName(PairIndex, false);
            }
        }
        return 0;
    }

    static bool CanIncDecByte(uint8_t From, uint8_t To)
    {
        return static_cast<uint8_t>(From + 1) == To || static_cast<uint8_t>(From - 1) == To;
    }

    static int32_t StackByteLoadCycles(char SourceRegisterName, bool bCanIncDec)
    {
        return SourceRegisterName != 0 || bCanIncDec ? 4 : 7;
    }

    static int32_t StackByteLoadCodeBytes(char SourceRegisterName, bool bCanIncDec)
    {
        return SourceRegisterName != 0 || bCanIncDec ? 1 : 2;
    }

    static uint8_t RegisterCode(char RegisterName)
    {
        switch (RegisterName)
        {
        case 'B': return 0;
        case 'C': return 1;
        case 'D': return 2;
        case 'E': return 3;
        case 'H': return 4;
        case 'L': return 5;
        case 'A': return 7;
        default: return 0;
        }
    }

    static int32_t ZXPixelOffsetFromByteXY(int32_t ByteX, int32_t Y)
    {
        return ((Y & 0xC0) << 5) | ((Y & 0x07) << 8) | ((Y & 0x38) << 2) | ByteX;
    }

    static int32_t CountZXColumnAddressLoads(const std::vector<int32_t>& Offsets)
    {
        if (Offsets.empty())
        {
            return 0;
        }

        int32_t Loads = 1;
        for (int32_t Index = 1; Index < (int32_t)Offsets.size(); ++Index)
        {
            if (Offsets[Index] != Offsets[Index - 1] + 0x0100)
            {
                ++Loads;
            }
        }
        return Loads;
    }

    static int32_t FutureWordScore(const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t NextPos, uint16_t Word)
    {
        int32_t Score = 0;
        for (int32_t Pos = NextPos; Pos >= StartOffset; Pos -= 2)
        {
            const uint16_t FutureWord = CodeGenerator::ReadWordLE(Data, Pos);
            if (FutureWord == Word)
            {
                Score += 4;
            }
            else
            {
                if (WordHigh(FutureWord) == WordHigh(Word))
                {
                    Score += 1;
                }
                if (WordLow(FutureWord) == WordLow(Word))
                {
                    Score += 1;
                }
            }
        }
        return Score;
    }

    static int32_t SelectPairToReplace(const FStackPairValue* Pairs, const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t NextPos)
    {
        for (int32_t PairIndex = 0; PairIndex < 3; ++PairIndex)
        {
            if (!Pairs[PairIndex].bHasValue)
            {
                return PairIndex;
            }
        }

        int32_t BestPairIndex = 0;
        int32_t BestScore = FutureWordScore(Data, StartOffset, NextPos, Pairs[0].Value);
        for (int32_t PairIndex = 1; PairIndex < 3; ++PairIndex)
        {
            const int32_t Score = FutureWordScore(Data, StartOffset, NextPos, Pairs[PairIndex].Value);
            if (Score < BestScore)
            {
                BestScore = Score;
                BestPairIndex = PairIndex;
            }
        }
        return BestPairIndex;
    }

    static FStackPairLoadChoice ChooseStackPairLoad(const FStackPairValue* Pairs, uint16_t Word, const std::vector<uint8_t>& Data, int32_t StartOffset, int32_t NextPos)
    {
        for (int32_t PairIndex = 0; PairIndex < 3; ++PairIndex)
        {
            if (Pairs[PairIndex].bHasValue && Pairs[PairIndex].Value == Word)
            {
                return { PairIndex, EStackPairLoadKind::None, false, false, 0, 0, 0, 0 };
            }
        }

        int32_t BestPartialPairIndex = INDEX_NONE;
        EStackPairLoadKind BestPartialKind = EStackPairLoadKind::None;
        char BestPartialSourceRegisterName = 0;
        int32_t BestPartialPreserveScore = (std::numeric_limits<int32_t>::max)();
        for (int32_t PairIndex = 0; PairIndex < 3; ++PairIndex)
        {
            if (!Pairs[PairIndex].bHasValue)
            {
                continue;
            }

            EStackPairLoadKind PartialKind = EStackPairLoadKind::None;
            char SourceRegisterName = 0;
            if (WordHigh(Pairs[PairIndex].Value) == WordHigh(Word))
            {
                PartialKind = EStackPairLoadKind::Low;
                SourceRegisterName = FindKnownByteRegister(Pairs, WordLow(Word));
            }
            else if (WordLow(Pairs[PairIndex].Value) == WordLow(Word))
            {
                PartialKind = EStackPairLoadKind::High;
                SourceRegisterName = FindKnownByteRegister(Pairs, WordHigh(Word));
            }

            if (PartialKind == EStackPairLoadKind::None)
            {
                continue;
            }

            const int32_t PreserveScore = FutureWordScore(Data, StartOffset, NextPos, Pairs[PairIndex].Value);
            if (PreserveScore < BestPartialPreserveScore)
            {
                BestPartialPairIndex = PairIndex;
                BestPartialKind = PartialKind;
                BestPartialSourceRegisterName = SourceRegisterName;
                BestPartialPreserveScore = PreserveScore;
            }
        }

        if (BestPartialPairIndex != INDEX_NONE)
        {
            const uint8_t CurrentByte = BestPartialKind == EStackPairLoadKind::High ?
                WordHigh(Pairs[BestPartialPairIndex].Value) :
                WordLow(Pairs[BestPartialPairIndex].Value);
            const uint8_t TargetByte = BestPartialKind == EStackPairLoadKind::High ?
                WordHigh(Word) :
                WordLow(Word);
            const bool bCanIncDec = CanIncDecByte(CurrentByte, TargetByte);

            return
            {
                BestPartialPairIndex,
                BestPartialKind,
                BestPartialKind == EStackPairLoadKind::High && BestPartialSourceRegisterName != 0,
                BestPartialKind == EStackPairLoadKind::Low && BestPartialSourceRegisterName != 0,
                BestPartialKind == EStackPairLoadKind::High ? BestPartialSourceRegisterName : 0,
                BestPartialKind == EStackPairLoadKind::Low ? BestPartialSourceRegisterName : 0,
                StackByteLoadCycles(BestPartialSourceRegisterName, bCanIncDec),
                StackByteLoadCodeBytes(BestPartialSourceRegisterName, bCanIncDec)
            };
        }

        const char HighSourceRegisterName = FindKnownByteRegister(Pairs, WordHigh(Word));
        const char LowSourceRegisterName = FindKnownByteRegister(Pairs, WordLow(Word));
        if (HighSourceRegisterName != 0 && LowSourceRegisterName != 0)
        {
            const int32_t PairIndex = SelectPairToReplace(Pairs, Data, StartOffset, NextPos);
            if (HighSourceRegisterName == StackPairByteRegisterName(PairIndex, false) &&
                LowSourceRegisterName == StackPairByteRegisterName(PairIndex, true))
            {
                return { PairIndex, EStackPairLoadKind::Full, false, false, 0, 0, 10, 3 };
            }
            return
            {
                PairIndex,
                EStackPairLoadKind::Bytes,
                true,
                true,
                HighSourceRegisterName,
                LowSourceRegisterName,
                8,
                2
            };
        }

        const int32_t PairIndex = SelectPairToReplace(Pairs, Data, StartOffset, NextPos);
        return { PairIndex, EStackPairLoadKind::Full, false, false, 0, 0, 10, 3 };
    }

    static void ApplyStackPairLoadChoice(FStackPairValue* Pairs, const FStackPairLoadChoice& Choice, uint16_t Word)
    {
        Pairs[Choice.PairIndex].bHasValue = true;
        Pairs[Choice.PairIndex].Value = Word;
    }

    static char GetPreferredRegisterNameForValue(const CodeGenerator::FAnalysis& Analysis, uint8_t Value)
    {
        if (Analysis.bHasPreferredBC)
        {
            if (Value == Analysis.PreferredB)
            {
                return 'B';
            }
            if (Value == Analysis.PreferredC)
            {
                return 'C';
            }
        }
        if (Analysis.bHasPreferredDE)
        {
            if (Value == Analysis.PreferredD)
            {
                return 'D';
            }
            if (Value == Analysis.PreferredE)
            {
                return 'E';
            }
        }
        return 0;
    }

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
    case EOpKind::ZXColumnBytes:            return "ZX_COLUMN_BYTES";
    case EOpKind::ZXColumnSameByte:         return "ZX_COLUMN_SAME_BYTE";
    case EOpKind::ZXColumnSameByteReg:      return "ZX_COLUMN_SAME_BYTE_REG";
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
    return std::format("DrawFrame_{}", FrameIndex);
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

    FCandidate ScoredCandidate = Candidate;
    auto AddByteFrequency = [&Analysis](int32_t Offset)
    {
        if (!IsValidOffset(Offset) || Analysis.ByteFrequency.size() != 256)
        {
            return 0;
        }
        return Analysis.ByteFrequency[Analysis.Data[Offset]];
    };

    auto AddWordFrequency = [&Analysis](int32_t Offset)
    {
        if (Offset < 0 || Offset + 1 >= ZX_SCREEN_SIZE || Analysis.WordFrequency.size() != 0x10000)
        {
            return 0;
        }
        return Analysis.WordFrequency[ReadWordLE(Analysis.Data, Offset)];
    };

    switch (ScoredCandidate.Kind)
    {
    case EOpKind::ByteAbsA:
    case EOpKind::ByteHLImm:
        ScoredCandidate.FrequencyScore = AddByteFrequency(ScoredCandidate.StartOffset);
        break;

    case EOpKind::WordAbsHL:
        ScoredCandidate.FrequencyScore = AddWordFrequency(ScoredCandidate.StartOffset) * 2;
        break;

    case EOpKind::StackBlock:
    {
        int32_t Score = 0;
        for (int32_t Offset = ScoredCandidate.StartOffset; Offset + 1 < ScoredCandidate.EndOffset; Offset += 2)
        {
            Score += AddWordFrequency(Offset) * 2;
        }
        ScoredCandidate.FrequencyScore = Score;
        break;
    }

    case EOpKind::RepeatWordAbsHL:
    case EOpKind::RepeatWordStack:
    case EOpKind::VerticalRepeatWordAbsHL:
        ScoredCandidate.FrequencyScore = AddWordFrequency(ScoredCandidate.StartOffset) * ScoredCandidate.WriteBytes;
        break;

    case EOpKind::HorizontalSameByteIncL:
    case EOpKind::HorizontalSameByteDecL:
    case EOpKind::HorizontalSameByteRegIncL:
    case EOpKind::HorizontalSameByteRegDecL:
    case EOpKind::VerticalSameByteIncH:
    case EOpKind::VerticalSameByteDecH:
    case EOpKind::VerticalSameByteRegIncH:
    case EOpKind::VerticalSameByteRegDecH:
    case EOpKind::ZXColumnSameByte:
    case EOpKind::ZXColumnSameByteReg:
        ScoredCandidate.FrequencyScore = AddByteFrequency(ScoredCandidate.StartOffset) * ScoredCandidate.WriteBytes;
        break;

    case EOpKind::VerticalBytesIncH:
    case EOpKind::VerticalBytesDecH:
    case EOpKind::ZXColumnBytes:
    {
        int32_t Score = 0;
        if (ScoredCandidate.Linear)
        {
            for (int32_t Offset = ScoredCandidate.StartOffset; Offset < ScoredCandidate.EndOffset; ++Offset)
            {
                Score += AddByteFrequency(Offset);
            }
        }
        else
        {
            for (int32_t Offset : ScoredCandidate.CoveredOffsets)
            {
                Score += AddByteFrequency(Offset);
            }
        }
        ScoredCandidate.FrequencyScore = Score;
        break;
    }

    default:
        ScoredCandidate.FrequencyScore = 0;
        break;
    }

    const int32_t ID = static_cast<int32_t>(Analysis.Candidates.size());
    Analysis.Candidates.push_back(ScoredCandidate);
    Analysis.StartsAt[ScoredCandidate.StartOffset].push_back(ID);
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

    // LD A, n       7 / 2 bytes or XOR A 4 / 1 byte for zero
    // LD (nn), A   13 / 3 bytes
    C.Cycles = (C.ByteValue == 0x00 ? 4 : 7) + 13;
    C.CodeBytes = (C.ByteValue == 0x00 ? 1 : 2) + 3;

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

    FStackPairValue Pairs[3] =
    {
        { false, 0 },
        { false, 0 },
        { false, 0 },
    };

    // PUSH пишет назад, поэтому пары читаем с конца блока.
    for (int32_t Pos = StartOffset + ByteLength - 2; Pos >= StartOffset; Pos -= 2)
    {
        const uint16_t Word = ReadWordLE(Data, Pos);
        const FStackPairLoadChoice Choice = ChooseStackPairLoad(Pairs, Word, Data, StartOffset, Pos - 2);
        C.Cycles += Choice.Cycles;
        C.CodeBytes += Choice.CodeBytes;
        ApplyStackPairLoadChoice(Pairs, Choice, Word);

        // PUSH rr
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
    // LD A, n            7 / 2 or XOR A 4 / 1 for zero
    // LD (HL), A         7 / 1
    // далее:
    // INC L              4 / 1
    // LD (HL), A         7 / 1
    C.Cycles = 10 + (C.ByteValue == 0x00 ? 4 : 7) + 7 + (Length - 1) * (4 + 7);
    C.CodeBytes = 3 + (C.ByteValue == 0x00 ? 1 : 2) + 1 + (Length - 1) * 2;

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
    C.RequiresB = RegisterName == 'B';
    C.RequiresC = RegisterName == 'C';
    C.RequiresD = RegisterName == 'D';
    C.RequiresE = RegisterName == 'E';
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

    // LD HL, addr / LD A, n or XOR A / LD (HL), A / INC H...
    C.Cycles = 10 + (C.ByteValue == 0x00 ? 4 : 7) + 7 + (Count - 1) * (4 + 7);
    C.CodeBytes = 3 + (C.ByteValue == 0x00 ? 1 : 2) + 1 + (Count - 1) * 2;

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

    C.Cycles = 10 + (C.ByteValue == 0x00 ? 4 : 7) + 7 + (Count - 1) * (4 + 7);
    C.CodeBytes = 3 + (C.ByteValue == 0x00 ? 1 : 2) + 1 + (Count - 1) * 2;

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
    C.RequiresB = RegisterName == 'B';
    C.RequiresC = RegisterName == 'C';
    C.RequiresD = RegisterName == 'D';
    C.RequiresE = RegisterName == 'E';
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

CodeGenerator::FCandidate CodeGenerator::MakeZXColumnBytes(const std::vector<uint8_t>& Data, int32_t ByteX, int32_t StartY, int32_t Count)
{
    FCandidate C;
    C.Kind = EOpKind::ZXColumnBytes;
    C.StartOffset = ZXPixelOffsetFromByteXY(ByteX, StartY);
    C.EndOffset = C.StartOffset + 1;
    C.StartAddr = AddrOf(C.StartOffset);
    C.WriteBytes = Count;
    C.Linear = false;
    C.Count = Count;
    C.Width = 1;

    C.CoveredOffsets.reserve(Count);
    for (int32_t i = 0; i < Count; ++i)
    {
        C.CoveredOffsets.push_back(ZXPixelOffsetFromByteXY(ByteX, StartY + i));
    }

    const int32_t AddressLoads = CountZXColumnAddressLoads(C.CoveredOffsets);
    C.Cycles = AddressLoads * 10 + Count * 10 + (Count - AddressLoads) * 4;
    C.CodeBytes = AddressLoads * 3 + Count * 2 + (Count - AddressLoads);
    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeZXColumnSameByte(const std::vector<uint8_t>& Data, int32_t ByteX, int32_t StartY, int32_t Count)
{
    FCandidate C = MakeZXColumnBytes(Data, ByteX, StartY, Count);
    C.Kind = EOpKind::ZXColumnSameByte;
    C.ByteValue = Data[C.CoveredOffsets[0]];

    const int32_t AddressLoads = CountZXColumnAddressLoads(C.CoveredOffsets);
    C.Cycles = AddressLoads * 10 + (C.ByteValue == 0x00 ? 4 : 7) + Count * 7 + (Count - AddressLoads) * 4;
    C.CodeBytes = AddressLoads * 3 + (C.ByteValue == 0x00 ? 1 : 2) + Count + (Count - AddressLoads);
    return C;
}

CodeGenerator::FCandidate CodeGenerator::MakeZXColumnSameByteReg(const std::vector<uint8_t>& Data, int32_t ByteX, int32_t StartY, int32_t Count, char RegisterName)
{
    FCandidate C = MakeZXColumnSameByte(Data, ByteX, StartY, Count);
    C.Kind = EOpKind::ZXColumnSameByteReg;
    C.RegisterName = RegisterName;
    C.RequiresB = RegisterName == 'B';
    C.RequiresC = RegisterName == 'C';
    C.RequiresD = RegisterName == 'D';
    C.RequiresE = RegisterName == 'E';

    const int32_t AddressLoads = CountZXColumnAddressLoads(C.CoveredOffsets);
    C.Cycles = AddressLoads * 10 + Count * 7 + (Count - AddressLoads) * 4;
    C.CodeBytes = AddressLoads * 3 + Count + (Count - AddressLoads);
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
    Analysis.ByteFrequency.assign(256, 0);
    Analysis.WordFrequency.assign(0x10000, 0);

    ProgressSet(Progress, 0, 100);
    if (ProgressCancelled(Progress))
    {
        OutError = "BuildAnalysis: cancelled";
        return false;
    }

    for (int32_t Offset = 0; Offset < ZX_SCREEN_SIZE; ++Offset)
    {
        if (Analysis.Dirty[Offset])
        {
            ++Analysis.ByteFrequency[Data[Offset]];
        }

        if (Offset + 1 < ZX_SCREEN_SIZE && RangeIsDirty(Analysis.Dirty, Offset, Offset + 2))
        {
            ++Analysis.WordFrequency[ReadWordLE(Data, Offset)];
        }

        if ((Offset & 0x3FF) == 0 && ProgressCancelled(Progress))
        {
            OutError = "BuildAnalysis: cancelled";
            return false;
        }
    }

    if (Options.EnableRegisterConstants)
    {
        int32_t BestValues[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
        for (int32_t Value = 0; Value < 256; ++Value)
        {
            for (int32_t Index = 0; Index < 4; ++Index)
            {
                if (BestValues[Index] != INDEX_NONE && Analysis.ByteFrequency[Value] <= Analysis.ByteFrequency[BestValues[Index]])
                {
                    continue;
                }

                bool bAlreadySelected = false;
                for (int32_t ExistingIndex = 0; ExistingIndex < Index; ++ExistingIndex)
                {
                    if (BestValues[ExistingIndex] == Value)
                    {
                        bAlreadySelected = true;
                        break;
                    }
                }
                if (bAlreadySelected)
                {
                    break;
                }

                for (int32_t ShiftIndex = 3; ShiftIndex > Index; --ShiftIndex)
                {
                    BestValues[ShiftIndex] = BestValues[ShiftIndex - 1];
                }
                BestValues[Index] = Value;
                break;
            }
        }

        if (BestValues[0] != INDEX_NONE && Analysis.ByteFrequency[BestValues[0]] > 0)
        {
            Analysis.bHasPreferredBC = true;
            Analysis.PreferredB = static_cast<uint8_t>(BestValues[0]);
            Analysis.PreferredC = static_cast<uint8_t>(BestValues[1] == INDEX_NONE ? BestValues[0] : BestValues[1]);
        }
        if (BestValues[2] != INDEX_NONE && Analysis.ByteFrequency[BestValues[2]] > 0)
        {
            Analysis.bHasPreferredDE = true;
            Analysis.PreferredD = static_cast<uint8_t>(BestValues[2]);
            Analysis.PreferredE = static_cast<uint8_t>(BestValues[3] == INDEX_NONE ? BestValues[2] : BestValues[3]);
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

            if (Options.EnableRegisterConstants && (Analysis.bHasPreferredBC || Analysis.bHasPreferredDE))
            {
                const char RegisterName = GetPreferredRegisterNameForValue(Analysis, Value);
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

                    if (Options.EnableRegisterConstants && (Analysis.bHasPreferredBC || Analysis.bHasPreferredDE))
                    {
                        const char RegisterName = GetPreferredRegisterNameForValue(Analysis, Value);
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

        for (int32_t ByteX = 0; ByteX < 32; ++ByteX)
        {
            int32_t Y = 0;
            while (Y < 192)
            {
                while (Y < 192 && !Analysis.Dirty[ZXPixelOffsetFromByteXY(ByteX, Y)])
                {
                    ++Y;
                }

                const int32_t StartY = Y;
                while (Y < 192 && Analysis.Dirty[ZXPixelOffsetFromByteXY(ByteX, Y)])
                {
                    ++Y;
                }

                const int32_t Count = Y - StartY;
                if (Count >= 3)
                {
                    AddCandidate(Analysis, MakeZXColumnBytes(Data, ByteX, StartY, Count));

                    for (int32_t SameStartY = StartY; SameStartY < StartY + Count; ++SameStartY)
                    {
                        const uint8_t Value = Data[ZXPixelOffsetFromByteXY(ByteX, SameStartY)];
                        int32_t SameCount = 1;
                        while (SameStartY + SameCount < StartY + Count &&
                            Data[ZXPixelOffsetFromByteXY(ByteX, SameStartY + SameCount)] == Value)
                        {
                            ++SameCount;
                        }

                        if (SameCount >= 3)
                        {
                            AddCandidate(Analysis, MakeZXColumnSameByte(Data, ByteX, SameStartY, SameCount));

                            if (Options.EnableRegisterConstants && (Analysis.bHasPreferredBC || Analysis.bHasPreferredDE))
                            {
                                const char RegisterName = GetPreferredRegisterNameForValue(Analysis, Value);
                                if (RegisterName != 0)
                                {
                                    AddCandidate(Analysis, MakeZXColumnSameByteReg(Data, ByteX, SameStartY, SameCount, RegisterName));
                                }
                            }
                        }

                        SameStartY += max(0, SameCount - 1);
                    }
                }
            }

            if ((ByteX & 0x07) == 0 && ProgressCancelled(Progress))
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

    void AppendSizeDelta(std::ostringstream& Out, int32_t OutputBytes, int32_t BaseBytes)
    {
        Out << OutputBytes << " bytes";
        if (BaseBytes <= 0)
        {
            Out << " (n/a)";
            return;
        }

        const int32_t DeltaBytes = OutputBytes - BaseBytes;
        const int64_t RatioHundredths = (static_cast<int64_t>(OutputBytes) * 100 + BaseBytes / 2) / BaseBytes;
        const int64_t DeltaPercentTenths = (static_cast<int64_t>(std::abs(DeltaBytes)) * 1000 + BaseBytes / 2) / BaseBytes;

        Out << " (x" << (RatioHundredths / 100) << "." << std::setw(2) << std::setfill('0') << (RatioHundredths % 100) << std::setfill(' ') << ", delta ";
        if (DeltaBytes > 0)
        {
            Out << "+";
        }
        else if (DeltaBytes < 0)
        {
            Out << "-";
        }
        Out << (DeltaPercentTenths / 10) << "." << (DeltaPercentTenths % 10) << "% vs active)";
    }

    void AppendPercentTenths(std::ostringstream& Out, int32_t Value, int32_t Base)
    {
        if (Base <= 0)
        {
            Out << "n/a";
            return;
        }

        const int64_t PercentTenths = (static_cast<int64_t>(Value) * 1000 + Base / 2) / Base;
        Out << (PercentTenths / 10) << "." << (PercentTenths % 10) << "%";
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

    bool CandidateUsesStack(const CodeGenerator::FCandidate& Candidate)
    {
        return Candidate.Kind == CodeGenerator::EOpKind::StackBlock ||
            Candidate.Kind == CodeGenerator::EOpKind::RepeatWordStack;
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
        std::vector<int32_t> DPCycles(N + 1, 0);
        std::vector<int32_t> DPCodeBytes(N + 1, 0);
        std::vector<int32_t> Choice(N + 1, -1);

        DP[N] = 0;

        for (int32_t i = N - 1; i >= 0; --i)
        {
            if (!Dirty[i])
            {
                DP[i] = DP[i + 1];
                DPCycles[i] = DPCycles[i + 1];
                DPCodeBytes[i] = DPCodeBytes[i + 1];
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
                const int32_t Cycles = Candidate.Cycles + DPCycles[Candidate.EndOffset];
                const int32_t CodeBytes = Candidate.CodeBytes + DPCodeBytes[Candidate.EndOffset];
                const bool bBetterCost = Cost < DP[i];
                bool bBetterTie = false;
                if (Cost == DP[i] && Choice[i] >= 0)
                {
                    bBetterTie =
                        CodeBytes < DPCodeBytes[i] ||
                        (CodeBytes == DPCodeBytes[i] && Cycles < DPCycles[i]);
                }
                if (bBetterCost || bBetterTie)
                {
                    DP[i] = Cost;
                    DPCycles[i] = Cycles;
                    DPCodeBytes[i] = CodeBytes;
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

static bool StateRegisterByte(const CodeGenerator::FEmitState& State, char RegisterName, uint8_t& OutValue);

bool CodeGenerator::OptimizePlan(const FAnalysis& Analysis, const FOptions& Options, FPlan& OutPlan, std::string& OutError, const FProgressInfo* Progress)
{
    OutPlan = FPlan();

    struct FSearchState
    {
        std::vector<uint8_t> Remaining;
        std::vector<int32_t> CandidateIds;
        FEmitState EmitState;
        int32_t RemainingCount = 0;
        int32_t Cycles = 0;
        int32_t CodeBytes = 0;
        bool UsesB = false;
        bool UsesC = false;
        bool UsesD = false;
        bool UsesE = false;
        bool UsesStack = false;
    };

    auto ScoreState = [&Options](const FSearchState& State)
    {
        return ScorePlan(State.Cycles, State.CodeBytes, Options);
    };
    auto IsBetterState = [&ScoreState](const FSearchState& A, const FSearchState& B)
    {
        const int64_t ScoreA = ScoreState(A);
        const int64_t ScoreB = ScoreState(B);
        if (ScoreA != ScoreB)
        {
            return ScoreA < ScoreB;
        }
        if (A.CodeBytes != B.CodeBytes)
        {
            return A.CodeBytes < B.CodeBytes;
        }
        if (A.Cycles != B.Cycles)
        {
            return A.Cycles < B.Cycles;
        }
        return false;
    };

    auto AddStackPlanOverheadIfNeeded = [&Options](FSearchState& State)
    {
        if (State.UsesStack)
        {
            return;
        }

        AddStackOverheadIfNeeded(State.Cycles, State.CodeBytes, Options);
        State.UsesStack = true;

        if (Options.PreserveSP)
        {
            State.EmitState.bHasHL = false;
            State.EmitState.bHasH = false;
            State.EmitState.bHasL = false;
            State.EmitState.bHasDE = false;
            State.EmitState.bHasBC = false;
            State.EmitState.bHasB = false;
            State.EmitState.bHasC = false;
            State.EmitState.bHasD = false;
            State.EmitState.bHasE = false;
            State.EmitState.bHasSP = true;
            State.EmitState.SP = Options.StackTopAddress;
        }
    };

    auto EvaluateCandidateTransition = [&Analysis](const FSearchState& State, const FCandidate& Candidate, FEmitState& OutEmitState, int32_t& OutCycles, int32_t& OutCodeBytes, std::string& OutTransitionError)
    {
        FEmitOutput EvalOut;
        OutEmitState = State.EmitState;
        if (!CodeGenerator::EmitCandidate(EvalOut, Candidate, Analysis.Data, OutEmitState, OutTransitionError))
        {
            return false;
        }

        OutCycles = EvalOut.Cycles;
        OutCodeBytes = static_cast<int32_t>(EvalOut.Code.size());
        return true;
    };

    auto ApplyCandidateTransition = [&](FSearchState& State, const FCandidate& Candidate, std::string& OutTransitionError)
    {
        if (!State.UsesStack && CandidateUsesStack(Candidate))
        {
            AddStackPlanOverheadIfNeeded(State);
        }

        FEmitState NextEmitState;
        int32_t CandidateCycles = 0;
        int32_t CandidateCodeBytes = 0;
        if (!EvaluateCandidateTransition(State, Candidate, NextEmitState, CandidateCycles, CandidateCodeBytes, OutTransitionError))
        {
            return false;
        }

        State.Cycles += CandidateCycles;
        State.CodeBytes += CandidateCodeBytes;
        State.EmitState = NextEmitState;
        State.UsesB = State.UsesB || Candidate.RequiresB;
        State.UsesC = State.UsesC || Candidate.RequiresC;
        State.UsesD = State.UsesD || Candidate.RequiresD;
        State.UsesE = State.UsesE || Candidate.RequiresE;
        return true;
    };

    const int32_t BeamWidth = max(1, Options.NonLinearBeamWidth);
    const int32_t MaxCandidatesToEvaluatePerPass = max(1, Options.MaxNonLinearCandidatesToEvaluatePerPass);
    const int32_t InitialDirty = CountDirtyBytes(Analysis.Dirty);

    std::vector<FSearchState> Beam;
    Beam.push_back(FSearchState{});
    Beam.back().Remaining = Analysis.Dirty;
    Beam.back().RemainingCount = InitialDirty;
    if (Analysis.bHasPreferredBC)
    {
        Beam.back().EmitState.bHasPreferredBC = true;
        Beam.back().EmitState.PreferredBC = static_cast<uint16_t>((Analysis.PreferredB << 8) | Analysis.PreferredC);
    }
    if (Analysis.bHasPreferredDE)
    {
        Beam.back().EmitState.bHasPreferredDE = true;
        Beam.back().EmitState.PreferredDE = static_cast<uint16_t>((Analysis.PreferredD << 8) | Analysis.PreferredE);
    }

    bool bHasBest = false;
    FSearchState BestFinished;

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
                if (!bHasBest || IsBetterState(State, BestFinished))
                {
                    BestFinished = State;
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

            FSearchState Finished = State;
            Finished.CandidateIds.insert(Finished.CandidateIds.end(), LinearIds.begin(), LinearIds.end());
            Finished.RemainingCount = 0;
            for (int32_t LinearID : LinearIds)
            {
                const FCandidate& LinearCandidate = Analysis.Candidates[LinearID];
                if (!ApplyCandidateTransition(Finished, LinearCandidate, OutError))
                {
                    return false;
                }
            }

            if (!bHasBest || IsBetterState(Finished, BestFinished))
            {
                BestFinished = Finished;
                bHasBest = true;
            }

            struct FByteAbsAGroup
            {
                uint8_t Value;
                std::vector<int32_t> CandidateIds;
                int64_t LinearScore = 0;
            };

            FByteAbsAGroup ByteGroups[256];
            for (int32_t Value = 0; Value < 256; ++Value)
            {
                ByteGroups[Value].Value = static_cast<uint8_t>(Value);
            }

            for (int32_t LinearID : LinearIds)
            {
                const FCandidate& LinearCandidate = Analysis.Candidates[LinearID];
                if (LinearCandidate.Kind != EOpKind::ByteAbsA)
                {
                    continue;
                }

                FByteAbsAGroup& Group = ByteGroups[LinearCandidate.ByteValue];
                Group.CandidateIds.push_back(LinearID);
                Group.LinearScore += ScoreCandidate(LinearCandidate, Options);
            }

            for (FByteAbsAGroup& Group : ByteGroups)
            {
                if (Group.CandidateIds.empty())
                {
                    continue;
                }

                uint8_t CurrentA = 0;
                const bool bAAlreadyHasValue = StateRegisterByte(State.EmitState, 'A', CurrentA) && CurrentA == Group.Value;
                if (Group.CandidateIds.size() < 2 && !bAAlreadyHasValue)
                {
                    continue;
                }

                FSearchState Child = State;
                int32_t GroupCycles = 0;
                int32_t GroupCodeBytes = 0;
                for (int32_t GroupCandidateID : Group.CandidateIds)
                {
                    const FCandidate& GroupCandidate = Analysis.Candidates[GroupCandidateID];
                    const int32_t BeforeCycles = Child.Cycles;
                    const int32_t BeforeCodeBytes = Child.CodeBytes;
                    Child.CandidateIds.push_back(GroupCandidateID);
                    if (!ApplyCandidateTransition(Child, GroupCandidate, OutError))
                    {
                        return false;
                    }
                    GroupCycles += Child.Cycles - BeforeCycles;
                    GroupCodeBytes += Child.CodeBytes - BeforeCodeBytes;
                    const int32_t Cleaned = MarkCandidateClean(Child.Remaining, GroupCandidate);
                    Child.RemainingCount = max(0, Child.RemainingCount - Cleaned);
                }

                if (ScorePlan(GroupCycles, GroupCodeBytes, Options) >= Group.LinearScore)
                {
                    continue;
                }

                NextBeam.push_back(std::move(Child));
            }

            struct FNonLinearProbe
            {
                int32_t ID;
                int64_t ApproxSaving;
                int32_t CandidateCodeBytes;
                int32_t CandidateCycles;
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
                if ((Candidate.Linear && Candidate.Kind != EOpKind::ByteAbsA) || !CandidateIsDirty(State.Remaining, Candidate))
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

                FEmitState ProbeEmitState;
                int32_t ProbeCycles = 0;
                int32_t ProbeCodeBytes = 0;
                if (!EvaluateCandidateTransition(State, Candidate, ProbeEmitState, ProbeCycles, ProbeCodeBytes, OutError))
                {
                    return false;
                }

                const int64_t CandidateCost = ScorePlan(ProbeCycles, ProbeCodeBytes, Options);
                if (CandidateCost >= OverlappedLinearCost)
                {
                    continue;
                }

                Probes.push_back({ ID, OverlappedLinearCost - CandidateCost, ProbeCodeBytes, ProbeCycles });
            }

            std::sort(Probes.begin(), Probes.end(),
                [](const FNonLinearProbe& A, const FNonLinearProbe& B)
                {
                    if (A.ApproxSaving != B.ApproxSaving)
                    {
                        return A.ApproxSaving > B.ApproxSaving;
                    }
                    if (A.CandidateCodeBytes != B.CandidateCodeBytes)
                    {
                        return A.CandidateCodeBytes < B.CandidateCodeBytes;
                    }
                    if (A.CandidateCycles != B.CandidateCycles)
                    {
                        return A.CandidateCycles < B.CandidateCycles;
                    }
                    return A.ID < B.ID;
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
                if (!ApplyCandidateTransition(Child, Candidate, OutError))
                {
                    return false;
                }

                const int32_t Cleaned = MarkCandidateClean(Child.Remaining, Candidate);
                Child.RemainingCount = max(0, Child.RemainingCount - Cleaned);
                NextBeam.push_back(std::move(Child));
            }
        }

        std::sort(NextBeam.begin(), NextBeam.end(),
            [&IsBetterState](const FSearchState& A, const FSearchState& B)
            {
                return IsBetterState(A, B);
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
    OutPlan.UsesB = BestFinished.UsesB;
    OutPlan.UsesC = BestFinished.UsesC;
    OutPlan.UsesD = BestFinished.UsesD;
    OutPlan.UsesE = BestFinished.UsesE;
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

static uint8_t IncRegisterOpcode(char RegisterName)
{
    switch (RegisterName)
    {
    case 'B': return 0x04;
    case 'C': return 0x0C;
    case 'D': return 0x14;
    case 'E': return 0x1C;
    case 'H': return 0x24;
    case 'L': return 0x2C;
    case 'A': return 0x3C;
    default: return 0x04;
    }
}

static uint8_t DecRegisterOpcode(char RegisterName)
{
    switch (RegisterName)
    {
    case 'B': return 0x05;
    case 'C': return 0x0D;
    case 'D': return 0x15;
    case 'E': return 0x1D;
    case 'H': return 0x25;
    case 'L': return 0x2D;
    case 'A': return 0x3D;
    default: return 0x05;
    }
}

static bool StateRegisterByte(const CodeGenerator::FEmitState& State, char RegisterName, uint8_t& OutValue)
{
    switch (RegisterName)
    {
    case 'A':
        if (!State.bHasA) { return false; }
        OutValue = State.A;
        return true;
    case 'B':
        if (!State.bHasB) { return false; }
        OutValue = WordHigh(State.BC);
        return true;
    case 'C':
        if (!State.bHasC) { return false; }
        OutValue = WordLow(State.BC);
        return true;
    case 'D':
        if (!State.bHasD) { return false; }
        OutValue = WordHigh(State.DE);
        return true;
    case 'E':
        if (!State.bHasE) { return false; }
        OutValue = WordLow(State.DE);
        return true;
    case 'H':
        if (!State.bHasH) { return false; }
        OutValue = WordHigh(State.HL);
        return true;
    case 'L':
        if (!State.bHasL) { return false; }
        OutValue = WordLow(State.HL);
        return true;
    default:
        return false;
    }
}

static char FindKnownRegisterWithByte(const CodeGenerator::FEmitState& State, uint8_t Value, char ExcludeRegisterName = 0)
{
    static constexpr char Registers[] = { 'A', 'B', 'C', 'D', 'E', 'H', 'L' };
    for (char RegisterName : Registers)
    {
        if (RegisterName == ExcludeRegisterName)
        {
            continue;
        }

        uint8_t RegisterValue = 0;
        if (StateRegisterByte(State, RegisterName, RegisterValue) && RegisterValue == Value)
        {
            return RegisterName;
        }
    }
    return 0;
}

static void EmitLoadRegisterByte(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, char TargetRegisterName, uint8_t Value);

static void EstimateRegisterByteLoadCost(const CodeGenerator::FEmitState& State, char TargetRegisterName, uint8_t Value, int32_t& OutCycles, int32_t& OutCodeBytes)
{
    uint8_t CurrentValue = 0;
    if (StateRegisterByte(State, TargetRegisterName, CurrentValue) && CurrentValue == Value)
    {
        OutCycles = 0;
        OutCodeBytes = 0;
        return;
    }

    if (FindKnownRegisterWithByte(State, Value, TargetRegisterName) != 0)
    {
        OutCycles = 4;
        OutCodeBytes = 1;
        return;
    }

    if (StateRegisterByte(State, TargetRegisterName, CurrentValue) &&
        (static_cast<uint8_t>(CurrentValue + 1) == Value || static_cast<uint8_t>(CurrentValue - 1) == Value))
    {
        OutCycles = 4;
        OutCodeBytes = 1;
        return;
    }

    OutCycles = 7;
    OutCodeBytes = 2;
}

void CodeGenerator::EmitLD_A(FEmitOutput& Out, FEmitState& State, uint8_t Value)
{
    if (!State.bHasA || State.A != Value)
    {
        if (Value == 0x00)
        {
            Out.Preview << "                XOR A\n";
            EmitByte(Out, 0xAF);
            Out.Cycles += 4;
        }
        else
        {
            const char SourceRegisterName = FindKnownRegisterWithByte(State, Value, 'A');
            if (SourceRegisterName != 0)
            {
                Out.Preview << "                LD A, " << SourceRegisterName << "\n";
                EmitByte(Out, static_cast<uint8_t>(0x40 + RegisterCode('A') * 8 + RegisterCode(SourceRegisterName)));
                Out.Cycles += 4;
            }
            else if (State.bHasA && static_cast<uint8_t>(State.A + 1) == Value)
            {
                Out.Preview << "                INC A\n";
                EmitByte(Out, IncRegisterOpcode('A'));
                Out.Cycles += 4;
            }
            else if (State.bHasA && static_cast<uint8_t>(State.A - 1) == Value)
            {
                Out.Preview << "                DEC A\n";
                EmitByte(Out, DecRegisterOpcode('A'));
                Out.Cycles += 4;
            }
            else
            {
                Out.Preview << "                LD A, " << Hex8(Value) << "\n";
                EmitByte(Out, 0x3E);
                EmitByte(Out, Value);
                Out.Cycles += 7;
            }
        }
        State.A = Value;
        State.bHasA = true;
    }
}

void CodeGenerator::EmitLD_HL(FEmitOutput& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasHL || State.HL != Value)
    {
        int32_t HighCycles = 0;
        int32_t HighCodeBytes = 0;
        int32_t LowCycles = 0;
        int32_t LowCodeBytes = 0;
        EstimateRegisterByteLoadCost(State, 'H', WordHigh(Value), HighCycles, HighCodeBytes);
        EstimateRegisterByteLoadCost(State, 'L', WordLow(Value), LowCycles, LowCodeBytes);
        const int32_t ByteLoadCycles = HighCycles + LowCycles;
        const int32_t ByteLoadCodeBytes = HighCodeBytes + LowCodeBytes;

        if (ByteLoadCodeBytes < 3 || (ByteLoadCodeBytes == 3 && ByteLoadCycles < 10))
        {
            EmitLoadRegisterByte(Out, State, 'H', WordHigh(Value));
            EmitLoadRegisterByte(Out, State, 'L', WordLow(Value));
            State.HL = Value;
            State.bHasHL = true;
            State.bHasH = true;
            State.bHasL = true;
        }
        else
        {
            Out.Preview << "                LD HL, " << Hex16(Value) << "\n";
            EmitByte(Out, 0x21);
            EmitWordLE(Out, Value);
            Out.Cycles += 10;
            State.HL = Value;
            State.bHasHL = true;
            State.bHasH = true;
            State.bHasL = true;
        }
    }
}

void CodeGenerator::EmitLD_BC(FEmitOutput& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasBC || State.BC != Value)
    {
        int32_t HighCycles = 0;
        int32_t HighCodeBytes = 0;
        int32_t LowCycles = 0;
        int32_t LowCodeBytes = 0;
        EstimateRegisterByteLoadCost(State, 'B', WordHigh(Value), HighCycles, HighCodeBytes);
        EstimateRegisterByteLoadCost(State, 'C', WordLow(Value), LowCycles, LowCodeBytes);
        const int32_t ByteLoadCycles = HighCycles + LowCycles;
        const int32_t ByteLoadCodeBytes = HighCodeBytes + LowCodeBytes;

        if (ByteLoadCodeBytes < 3 || (ByteLoadCodeBytes == 3 && ByteLoadCycles < 10))
        {
            EmitLoadRegisterByte(Out, State, 'B', WordHigh(Value));
            EmitLoadRegisterByte(Out, State, 'C', WordLow(Value));
            State.BC = Value;
            State.bHasBC = true;
            State.bHasB = true;
            State.bHasC = true;
        }
        else
        {
            Out.Preview << "                LD BC, " << Hex16(Value) << "\n";
            EmitByte(Out, 0x01);
            EmitWordLE(Out, Value);
            Out.Cycles += 10;
            State.BC = Value;
            State.bHasBC = true;
            State.bHasB = true;
            State.bHasC = true;
        }
    }
}

void CodeGenerator::EmitLD_DE(FEmitOutput& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasDE || State.DE != Value)
    {
        int32_t HighCycles = 0;
        int32_t HighCodeBytes = 0;
        int32_t LowCycles = 0;
        int32_t LowCodeBytes = 0;
        EstimateRegisterByteLoadCost(State, 'D', WordHigh(Value), HighCycles, HighCodeBytes);
        EstimateRegisterByteLoadCost(State, 'E', WordLow(Value), LowCycles, LowCodeBytes);
        const int32_t ByteLoadCycles = HighCycles + LowCycles;
        const int32_t ByteLoadCodeBytes = HighCodeBytes + LowCodeBytes;

        if (ByteLoadCodeBytes < 3 || (ByteLoadCodeBytes == 3 && ByteLoadCycles < 10))
        {
            EmitLoadRegisterByte(Out, State, 'D', WordHigh(Value));
            EmitLoadRegisterByte(Out, State, 'E', WordLow(Value));
            State.DE = Value;
            State.bHasDE = true;
            State.bHasD = true;
            State.bHasE = true;
        }
        else
        {
            Out.Preview << "                LD DE, " << Hex16(Value) << "\n";
            EmitByte(Out, 0x11);
            EmitWordLE(Out, Value);
            Out.Cycles += 10;
            State.DE = Value;
            State.bHasDE = true;
            State.bHasD = true;
            State.bHasE = true;
        }
    }
}

static void EmitLD_PairFull(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, int32_t PairIndex, uint16_t Value)
{
    switch (PairIndex)
    {
    case 0:
        CodeGenerator::EmitLD_HL(Out, State, Value);
        break;
    case 1:
        CodeGenerator::EmitLD_DE(Out, State, Value);
        break;
    case 2:
        CodeGenerator::EmitLD_BC(Out, State, Value);
        break;
    default:
        CodeGenerator::EmitLD_HL(Out, State, Value);
        break;
    }
}

static void EmitLD_Register(CodeGenerator::FEmitOutput& Out, char TargetRegisterName, char SourceRegisterName)
{
    Out.Preview << "                LD " << TargetRegisterName << ", " << SourceRegisterName << "\n";
    CodeGenerator::EmitByte(Out, static_cast<uint8_t>(0x40 + RegisterCode(TargetRegisterName) * 8 + RegisterCode(SourceRegisterName)));
    Out.Cycles += 4;
}

static void EmitLD_RegisterImmediate(CodeGenerator::FEmitOutput& Out, char TargetRegisterName, uint8_t Value)
{
    uint8_t Opcode = 0x00;
    switch (TargetRegisterName)
    {
    case 'B': Opcode = 0x06; break;
    case 'C': Opcode = 0x0E; break;
    case 'D': Opcode = 0x16; break;
    case 'E': Opcode = 0x1E; break;
    case 'H': Opcode = 0x26; break;
    case 'L': Opcode = 0x2E; break;
    case 'A': Opcode = 0x3E; break;
    default: Opcode = 0x06; break;
    }

    Out.Preview << "                LD " << TargetRegisterName << ", " << CodeGenerator::Hex8(Value) << "\n";
    CodeGenerator::EmitByte(Out, Opcode);
    CodeGenerator::EmitByte(Out, Value);
    Out.Cycles += 7;
}

static void SetStateRegisterByte(CodeGenerator::FEmitState& State, char RegisterName, uint8_t Value)
{
    switch (RegisterName)
    {
    case 'B':
        State.BC = static_cast<uint16_t>((State.BC & 0x00FF) | (Value << 8));
        State.bHasB = true;
        State.bHasBC = State.bHasB && State.bHasC;
        break;
    case 'C':
        State.BC = static_cast<uint16_t>((State.BC & 0xFF00) | Value);
        State.bHasC = true;
        State.bHasBC = State.bHasB && State.bHasC;
        break;
    case 'D':
        State.DE = static_cast<uint16_t>((State.DE & 0x00FF) | (Value << 8));
        State.bHasD = true;
        State.bHasDE = State.bHasD && State.bHasE;
        break;
    case 'E':
        State.DE = static_cast<uint16_t>((State.DE & 0xFF00) | Value);
        State.bHasE = true;
        State.bHasDE = State.bHasD && State.bHasE;
        break;
    case 'H':
        State.HL = static_cast<uint16_t>((State.HL & 0x00FF) | (Value << 8));
        State.bHasH = true;
        State.bHasHL = State.bHasH && State.bHasL;
        break;
    case 'L':
        State.HL = static_cast<uint16_t>((State.HL & 0xFF00) | Value);
        State.bHasL = true;
        State.bHasHL = State.bHasH && State.bHasL;
        break;
    case 'A':
        State.A = Value;
        State.bHasA = true;
        break;
    default:
        break;
    }
}

static void EmitLoadRegisterByte(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, char TargetRegisterName, uint8_t Value)
{
    uint8_t CurrentValue = 0;
    if (StateRegisterByte(State, TargetRegisterName, CurrentValue) && CurrentValue == Value)
    {
        return;
    }

    const char SourceRegisterName = FindKnownRegisterWithByte(State, Value, TargetRegisterName);
    if (SourceRegisterName != 0)
    {
        EmitLD_Register(Out, TargetRegisterName, SourceRegisterName);
        SetStateRegisterByte(State, TargetRegisterName, Value);
        return;
    }

    if (StateRegisterByte(State, TargetRegisterName, CurrentValue) && static_cast<uint8_t>(CurrentValue + 1) == Value)
    {
        Out.Preview << "                INC " << TargetRegisterName << "\n";
        CodeGenerator::EmitByte(Out, IncRegisterOpcode(TargetRegisterName));
        Out.Cycles += 4;
        SetStateRegisterByte(State, TargetRegisterName, Value);
        return;
    }

    if (StateRegisterByte(State, TargetRegisterName, CurrentValue) && static_cast<uint8_t>(CurrentValue - 1) == Value)
    {
        Out.Preview << "                DEC " << TargetRegisterName << "\n";
        CodeGenerator::EmitByte(Out, DecRegisterOpcode(TargetRegisterName));
        Out.Cycles += 4;
        SetStateRegisterByte(State, TargetRegisterName, Value);
        return;
    }

    EmitLD_RegisterImmediate(Out, TargetRegisterName, Value);
    SetStateRegisterByte(State, TargetRegisterName, Value);
}

static uint8_t GetPreferredRegisterValue(const CodeGenerator::FEmitState& State, char RegisterName)
{
    switch (RegisterName)
    {
    case 'B': return WordHigh(State.PreferredBC);
    case 'C': return WordLow(State.PreferredBC);
    case 'D': return WordHigh(State.PreferredDE);
    case 'E': return WordLow(State.PreferredDE);
    default: return 0;
    }
}

static bool RegisterHasPreferredValue(const CodeGenerator::FEmitState& State, char RegisterName)
{
    switch (RegisterName)
    {
    case 'B': return State.bHasB && WordHigh(State.BC) == WordHigh(State.PreferredBC);
    case 'C': return State.bHasC && WordLow(State.BC) == WordLow(State.PreferredBC);
    case 'D': return State.bHasD && WordHigh(State.DE) == WordHigh(State.PreferredDE);
    case 'E': return State.bHasE && WordLow(State.DE) == WordLow(State.PreferredDE);
    default: return false;
    }
}

static void EmitLD_PairByte(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, const FStackPairLoadChoice& Choice, uint16_t Value)
{
    if (Choice.LoadKind == EStackPairLoadKind::None)
    {
        return;
    }

    if (Choice.LoadKind == EStackPairLoadKind::Full)
    {
        EmitLD_PairFull(Out, State, Choice.PairIndex, Value);
        return;
    }

    auto EmitByteLoad = [&](bool bHigh, bool bFromRegister, char SourceRegisterName)
    {
        const char TargetRegisterName = StackPairByteRegisterName(Choice.PairIndex, bHigh);
        if (bFromRegister)
        {
            EmitLD_Register(Out, TargetRegisterName, SourceRegisterName);
        }
        else
        {
            EmitLoadRegisterByte(Out, State, TargetRegisterName, bHigh ? WordHigh(Value) : WordLow(Value));
        }
    };

    if (Choice.LoadKind == EStackPairLoadKind::High)
    {
        EmitByteLoad(true, Choice.bHighFromRegister, Choice.HighRegisterName);
    }
    else if (Choice.LoadKind == EStackPairLoadKind::Low)
    {
        EmitByteLoad(false, Choice.bLowFromRegister, Choice.LowRegisterName);
    }
    else if (Choice.LoadKind == EStackPairLoadKind::Bytes)
    {
        const char TargetHighRegisterName = StackPairByteRegisterName(Choice.PairIndex, true);
        if (Choice.LowRegisterName == TargetHighRegisterName)
        {
            EmitByteLoad(false, true, Choice.LowRegisterName);
            EmitByteLoad(true, true, Choice.HighRegisterName);
        }
        else
        {
            EmitByteLoad(true, true, Choice.HighRegisterName);
            EmitByteLoad(false, true, Choice.LowRegisterName);
        }
    }

    switch (Choice.PairIndex)
    {
    case 0:
        State.HL = Value;
        State.bHasHL = true;
        State.bHasH = true;
        State.bHasL = true;
        break;
    case 1:
        State.DE = Value;
        State.bHasDE = true;
        State.bHasD = true;
        State.bHasE = true;
        break;
    case 2:
        State.BC = Value;
        State.bHasBC = true;
        State.bHasB = true;
        State.bHasC = true;
        break;
    default:
        State.HL = Value;
        State.bHasHL = true;
        State.bHasH = true;
        State.bHasL = true;
        break;
    }
}

static void EmitPUSH_Pair(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, int32_t PairIndex)
{
    uint8_t Opcode = 0xE5;
    switch (PairIndex)
    {
    case 0:
        Opcode = 0xE5;
        break;
    case 1:
        Opcode = 0xD5;
        break;
    case 2:
        Opcode = 0xC5;
        break;
    default:
        Opcode = 0xE5;
        break;
    }

    Out.Preview << "                PUSH " << StackPairName(PairIndex) << "\n";
    CodeGenerator::EmitByte(Out, Opcode);
    Out.Cycles += 11;

    State.SP = static_cast<uint16_t>(State.SP - 2);
    State.bHasSP = true;
}

static void EmitEnsurePreferredRegister(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, char RegisterName)
{
    const bool bHasPreferred =
        ((RegisterName == 'B' || RegisterName == 'C') && State.bHasPreferredBC) ||
        ((RegisterName == 'D' || RegisterName == 'E') && State.bHasPreferredDE);
    if (!bHasPreferred || RegisterHasPreferredValue(State, RegisterName))
    {
        return;
    }

    const uint8_t Value = GetPreferredRegisterValue(State, RegisterName);
    EmitLoadRegisterByte(Out, State, RegisterName, Value);
}

static uint8_t StoreHLRegisterOpcode(char RegisterName)
{
    switch (RegisterName)
    {
    case 'B': return 0x70;
    case 'C': return 0x71;
    case 'D': return 0x72;
    case 'E': return 0x73;
    case 'H': return 0x74;
    case 'L': return 0x75;
    case 'A': return 0x77;
    default: return 0x70;
    }
}

static bool CandidateUsesAF(const CodeGenerator::FCandidate& Candidate)
{
    switch (Candidate.Kind)
    {
    case CodeGenerator::EOpKind::ByteAbsA:
    case CodeGenerator::EOpKind::HorizontalSameByteIncL:
    case CodeGenerator::EOpKind::HorizontalSameByteDecL:
    case CodeGenerator::EOpKind::VerticalSameByteIncH:
    case CodeGenerator::EOpKind::VerticalSameByteDecH:
    case CodeGenerator::EOpKind::ZXColumnSameByte:
        return true;
    default:
        return false;
    }
}

static bool CandidateUsesHL(const CodeGenerator::FCandidate& Candidate)
{
    return Candidate.Kind != CodeGenerator::EOpKind::ByteAbsA;
}

static void AppendRegisterName(std::ostringstream& Stream, bool& bFirst, const char* RegisterName)
{
    if (!bFirst)
    {
        Stream << ", ";
    }
    Stream << RegisterName;
    bFirst = false;
}

static int32_t ExtractFrameIndexFromLabelName(const std::string& LabelName)
{
    const size_t UnderscorePos = LabelName.find_last_of('_');
    if (UnderscorePos == std::string::npos || UnderscorePos + 1 >= LabelName.size())
    {
        return INDEX_NONE;
    }

    int32_t FrameIndex = 0;
    for (size_t Index = UnderscorePos + 1; Index < LabelName.size(); ++Index)
    {
        const char Character = LabelName[Index];
        if (Character < '0' || Character > '9')
        {
            return INDEX_NONE;
        }
        FrameIndex = FrameIndex * 10 + (Character - '0');
    }
    return FrameIndex;
}

static void EmitAsmHeader(
    CodeGenerator::FEmitOutput& Out,
    const CodeGenerator::FAnalysis& Analysis,
    const CodeGenerator::FPlan& Plan,
    const CodeGenerator::FOptions& Options,
    const std::string& LabelName)
{
    bool bUsesAF = false;
    bool bUsesHL = false;
    bool bUsesBC = Plan.UsesB || Plan.UsesC;
    bool bUsesDE = Plan.UsesD || Plan.UsesE;

    for (int32_t ID : Plan.CandidateIds)
    {
        const CodeGenerator::FCandidate& Candidate = Analysis.Candidates[ID];
        bUsesAF = bUsesAF || CandidateUsesAF(Candidate);
        bUsesHL = bUsesHL || CandidateUsesHL(Candidate);
        bUsesBC = bUsesBC || Candidate.Kind == CodeGenerator::EOpKind::StackBlock || Candidate.Kind == CodeGenerator::EOpKind::RepeatWordStack;
        bUsesDE = bUsesDE || Candidate.Kind == CodeGenerator::EOpKind::StackBlock || Candidate.Kind == CodeGenerator::EOpKind::RepeatWordStack;
    }

    const bool bUsesStack = Plan.UsesStack;
    const bool bPreserveSP = Options.PreserveSP && bUsesStack;
    bUsesHL = bUsesHL || bPreserveSP;
    bUsesBC = bUsesBC || bPreserveSP;
    bUsesDE = bUsesDE || bPreserveSP;

    std::ostringstream Corrupt;
    bool bFirstRegister = true;
    if (bUsesHL)
    {
        AppendRegisterName(Corrupt, bFirstRegister, "HL");
    }
    if (bUsesDE)
    {
        AppendRegisterName(Corrupt, bFirstRegister, "DE");
    }
    if (bUsesBC)
    {
        AppendRegisterName(Corrupt, bFirstRegister, "BC");
    }
    if (bUsesAF)
    {
        AppendRegisterName(Corrupt, bFirstRegister, "AF");
    }
    if (bUsesStack && !bPreserveSP)
    {
        AppendRegisterName(Corrupt, bFirstRegister, "SP");
    }
    if (bFirstRegister)
    {
        Corrupt << "none";
    }

    std::vector<int32_t> FrequentBytes;
    FrequentBytes.reserve(8);
    for (int32_t Value = 0; Value < 256; ++Value)
    {
        if (Analysis.ByteFrequency.size() != 256 || Analysis.ByteFrequency[Value] <= 0)
        {
            continue;
        }

        const int32_t InsertIndex = static_cast<int32_t>(std::find_if(
            FrequentBytes.begin(),
            FrequentBytes.end(),
            [&Analysis, Value](int32_t OtherValue)
            {
                return Analysis.ByteFrequency[Value] > Analysis.ByteFrequency[OtherValue];
            }) - FrequentBytes.begin());

        if (InsertIndex < 8)
        {
            FrequentBytes.insert(FrequentBytes.begin() + InsertIndex, Value);
            if (FrequentBytes.size() > 8)
            {
                FrequentBytes.resize(8);
            }
        }
    }

    Out.Preview << "; -----------------------------------------\n";
    Out.Preview << "; ZX Spectrum 6912 draw frame\n";
    Out.Preview << "; In:\n";
    Out.Preview << "; Out:\n";
    const int32_t FrameIndex = ExtractFrameIndexFromLabelName(LabelName);
    if (FrameIndex != INDEX_NONE)
    {
        Out.Preview << ";   Frame        - " << FrameIndex << "\n";
    }
    Out.Preview << ";   Function     - " << LabelName << "\n";
    Out.Preview << ";   Destination  - ZX memory destination " << CodeGenerator::Hex16(CodeGenerator::ZX_SCREEN_BASE)
        << ".." << CodeGenerator::Hex16(CodeGenerator::ZX_SCREEN_END - 1) << "\n";
    Out.Preview << ";   Value        - frame data, see generated operations below\n";
    const int32_t ActiveBytes = CountDirtyBytes(Analysis.Dirty);
    Out.Preview << ";   Size         - " << ActiveBytes << " active bytes\n";
    Out.Preview << ";   Screen cover - ";
    AppendPercentTenths(Out.Preview, ActiveBytes, CodeGenerator::ZX_SCREEN_SIZE);
    Out.Preview << " of 6912 bytes\n";
    Out.Preview << ";   Output size  - ";
    AppendSizeDelta(Out.Preview, Plan.TotalCodeBytes, ActiveBytes);
    Out.Preview << "\n";
    Out.Preview << ";   Cycles       - " << Plan.TotalCycles << "\n";
    Out.Preview << ";   Frame time   - ";
    AppendPercentTenths(Out.Preview, Plan.TotalCycles, 71680);
    Out.Preview << " (Pentagon 71680 cycles)\n";
    Out.Preview << ";   Operations   - " << Plan.CandidateIds.size() << "\n";
    Out.Preview << "; Corrupt:\n";
    Out.Preview << ";   " << Corrupt.str() << "\n";
    Out.Preview << "; Note:\n";
    Out.Preview << ";   Stack writes - " << (bUsesStack ? "yes" : "no")
        << ", preserve SP - " << (bPreserveSP ? "yes" : "no")
        << ", interrupts - " << (Options.DisableInterruptsForStack && bUsesStack ? "DI/EI" : "unchanged") << "\n";
    Out.Preview << ";   Register constants - ";
    if (Plan.UsesB || Plan.UsesC || Plan.UsesD || Plan.UsesE)
    {
        bool bFirst = true;
        if (Plan.UsesB)
        {
            Out.Preview << "B=" << CodeGenerator::Hex8(Analysis.PreferredB);
            bFirst = false;
        }
        if (Plan.UsesC)
        {
            Out.Preview << (bFirst ? "" : ", ") << "C=" << CodeGenerator::Hex8(Analysis.PreferredC);
            bFirst = false;
        }
        if (Plan.UsesD)
        {
            Out.Preview << (bFirst ? "" : ", ") << "D=" << CodeGenerator::Hex8(Analysis.PreferredD);
            bFirst = false;
        }
        if (Plan.UsesE)
        {
            Out.Preview << (bFirst ? "" : ", ") << "E=" << CodeGenerator::Hex8(Analysis.PreferredE);
        }
        Out.Preview << "\n";
    }
    else
    {
        Out.Preview << "none\n";
    }
    Out.Preview << ";   Frequent bytes - ";
    if (FrequentBytes.empty())
    {
        Out.Preview << "none";
    }
    else
    {
        for (int32_t Index = 0; Index < static_cast<int32_t>(FrequentBytes.size()); ++Index)
        {
            const int32_t Value = FrequentBytes[Index];
            if (Index > 0)
            {
                Out.Preview << ", ";
            }
            Out.Preview << CodeGenerator::Hex8(static_cast<uint8_t>(Value)) << " x" << Analysis.ByteFrequency[Value];
        }
    }
    Out.Preview << "\n";
    Out.Preview << "; -----------------------------------------\n\n";
}

static void EmitMoveHLToOffset(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, uint16_t& HL, int32_t Offset)
{
    const uint16_t Target = CodeGenerator::AddrOf(Offset);
    if (State.bHasHL && State.HL == Target)
    {
        HL = Target;
        return;
    }

    CodeGenerator::EmitLD_HL(Out, State, Target);
    HL = Target;
}

static void EmitStepHLToNextOffset(CodeGenerator::FEmitOutput& Out, CodeGenerator::FEmitState& State, uint16_t& HL, int32_t CurrentOffset, int32_t NextOffset)
{
    if (NextOffset == CurrentOffset + 0x0100)
    {
        Out.Preview << "                INC H\n";
        CodeGenerator::EmitByte(Out, 0x24);
        Out.Cycles += 4;
        HL = static_cast<uint16_t>(HL + 0x0100);
        State.HL = HL;
        State.bHasHL = true;
        State.bHasH = true;
        State.bHasL = true;
        return;
    }

    EmitMoveHLToOffset(Out, State, HL, NextOffset);
}

void CodeGenerator::EmitLD_SP(FEmitOutput& Out, FEmitState& State, uint16_t Value)
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
        EmitLD_A(Out, State, Candidate.ByteValue);
        Out.Preview << "                LD (" << Hex16(Candidate.StartAddr) << "), A\n";
        EmitByte(Out, 0x32);
        EmitWordLE(Out, Candidate.StartAddr);
        Out.Cycles += 13;
        break;
    }

    case EOpKind::ByteHLImm:
    {
        EmitLD_HL(Out, State, Candidate.StartAddr);
        Out.Preview << "                LD (HL), " << Hex8(Candidate.ByteValue) << "\n";
        EmitByte(Out, 0x36);
        EmitByte(Out, Candidate.ByteValue);
        Out.Cycles += 10;

        // HL не меняется.
        break;
    }

    case EOpKind::WordAbsHL:
    {
        EmitLD_HL(Out, State, Candidate.WordValue);
        Out.Preview << "                LD (" << Hex16(Candidate.StartAddr) << "), HL\n";
        EmitByte(Out, 0x22);
        EmitWordLE(Out, Candidate.StartAddr);
        Out.Cycles += 16;
        break;
    }

    case EOpKind::StackBlock:
    {
        const uint16_t endAddr = AddrOf(Candidate.EndOffset);
        EmitLD_SP(Out, State, endAddr);

        FStackPairValue Pairs[3] =
        {
            { State.bHasHL, State.HL },
            { State.bHasDE, State.DE },
            { State.bHasBC, State.BC },
        };
        for (int32_t Pos = Candidate.EndOffset - 2; Pos >= Candidate.StartOffset; Pos -= 2)
        {
            const uint16_t Word = ReadWordLE(Data, Pos);
            const FStackPairLoadChoice Choice = ChooseStackPairLoad(Pairs, Word, Data, Candidate.StartOffset, Pos - 2);
            EmitLD_PairByte(Out, State, Choice, Word);
            ApplyStackPairLoadChoice(Pairs, Choice, Word);
            EmitPUSH_Pair(Out, State, Choice.PairIndex);
        }

        break;
    }

    case EOpKind::RepeatWordAbsHL:
    {
        EmitLD_HL(Out, State, Candidate.WordValue);

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
        EmitLD_SP(Out, State, EndAddr);
        FStackPairValue Pairs[3] =
        {
            { State.bHasHL, State.HL },
            { State.bHasDE, State.DE },
            { State.bHasBC, State.BC },
        };
        const FStackPairLoadChoice Choice = ChooseStackPairLoad(Pairs, Candidate.WordValue, Data, Candidate.StartOffset, Candidate.StartOffset - 2);
        EmitLD_PairByte(Out, State, Choice, Candidate.WordValue);
        ApplyStackPairLoadChoice(Pairs, Choice, Candidate.WordValue);

        const int32_t PairCount = Candidate.WriteBytes / 2;
        for (int32_t i = 0; i < PairCount; ++i)
        {
            EmitPUSH_Pair(Out, State, Choice.PairIndex);
        }

        break;
    }

    case EOpKind::HorizontalSameByteIncL:
    case EOpKind::HorizontalSameByteDecL:
    {
        EmitLD_HL(Out, State, Candidate.StartAddr);
        EmitLD_A(Out, State, Candidate.ByteValue);

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
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::HorizontalSameByteRegIncL:
    case EOpKind::HorizontalSameByteRegDecL:
    {
        EmitEnsurePreferredRegister(Out, State, Candidate.RegisterName);
        EmitLD_HL(Out, State, Candidate.StartAddr);

        const uint8_t StoreOpcode = StoreHLRegisterOpcode(Candidate.RegisterName);
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
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::VerticalBytesIncH:
    case EOpKind::VerticalBytesDecH:
    {
        EmitLD_HL(Out, State, Candidate.StartAddr);

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
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::VerticalSameByteIncH:
    case EOpKind::VerticalSameByteDecH:
    {
        EmitLD_HL(Out, State, Candidate.StartAddr);
        EmitLD_A(Out, State, Candidate.ByteValue);

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
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::VerticalSameByteRegIncH:
    case EOpKind::VerticalSameByteRegDecH:
    {
        EmitEnsurePreferredRegister(Out, State, Candidate.RegisterName);
        EmitLD_HL(Out, State, Candidate.StartAddr);

        const bool bDec = Candidate.Kind == EOpKind::VerticalSameByteRegDecH;
        const uint8_t StoreOpcode = StoreHLRegisterOpcode(Candidate.RegisterName);
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
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::VerticalRepeatWordAbsHL:
    {
        EmitLD_HL(Out, State, Candidate.WordValue);

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

    case EOpKind::ZXColumnBytes:
    {
        uint16_t HL = Candidate.StartAddr;
        EmitMoveHLToOffset(Out, State, HL, Candidate.CoveredOffsets[0]);

        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            const int32_t Offset = Candidate.CoveredOffsets[i];
            Out.Preview << "                LD (HL), " << Hex8(Data[Offset]) << "\n";
            EmitByte(Out, 0x36);
            EmitByte(Out, Data[Offset]);
            Out.Cycles += 10;

            if (i + 1 < Candidate.Count)
            {
                EmitStepHLToNextOffset(Out, State, HL, Offset, Candidate.CoveredOffsets[i + 1]);
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::ZXColumnSameByte:
    {
        uint16_t HL = Candidate.StartAddr;
        EmitMoveHLToOffset(Out, State, HL, Candidate.CoveredOffsets[0]);
        EmitLD_A(Out, State, Candidate.ByteValue);

        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            const int32_t Offset = Candidate.CoveredOffsets[i];
            Out.Preview << "                LD (HL), A\n";
            EmitByte(Out, 0x77);
            Out.Cycles += 7;

            if (i + 1 < Candidate.Count)
            {
                EmitStepHLToNextOffset(Out, State, HL, Offset, Candidate.CoveredOffsets[i + 1]);
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        State.bHasH = true;
        State.bHasL = true;
        break;
    }

    case EOpKind::ZXColumnSameByteReg:
    {
        EmitEnsurePreferredRegister(Out, State, Candidate.RegisterName);

        uint16_t HL = Candidate.StartAddr;
        EmitMoveHLToOffset(Out, State, HL, Candidate.CoveredOffsets[0]);

        const uint8_t StoreOpcode = StoreHLRegisterOpcode(Candidate.RegisterName);
        for (int32_t i = 0; i < Candidate.Count; ++i)
        {
            const int32_t Offset = Candidate.CoveredOffsets[i];
            Out.Preview << "                LD (HL), " << Candidate.RegisterName << "\n";
            EmitByte(Out, StoreOpcode);
            Out.Cycles += 7;

            if (i + 1 < Candidate.Count)
            {
                EmitStepHLToNextOffset(Out, State, HL, Offset, Candidate.CoveredOffsets[i + 1]);
            }
        }

        State.HL = HL;
        State.bHasHL = true;
        State.bHasH = true;
        State.bHasL = true;
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

    EmitAsmHeader(Out, Analysis, Plan, Options, LabelName);
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
        State.bHasH = false;
        State.bHasL = false;
        State.bHasDE = false;
        State.bHasBC = false;
        State.bHasB = false;
        State.bHasC = false;
        State.bHasD = false;
        State.bHasE = false;
        State.bHasSP = true;
        State.SP = Options.StackTopAddress;
    }
    if (Analysis.bHasPreferredBC)
    {
        State.bHasPreferredBC = true;
        State.PreferredBC = static_cast<uint16_t>((Analysis.PreferredB << 8) | Analysis.PreferredC);
    }
    if (Analysis.bHasPreferredDE)
    {
        State.bHasPreferredDE = true;
        State.PreferredDE = static_cast<uint16_t>((Analysis.PreferredD << 8) | Analysis.PreferredE);
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
        case EOpKind::ZXColumnBytes:
        case EOpKind::ZXColumnSameByte:
        case EOpKind::ZXColumnSameByteReg:
            ++VertCount;
            break;
        }

        if (Candidate.RequiresB || Candidate.RequiresC || Candidate.RequiresD || Candidate.RequiresE)
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
