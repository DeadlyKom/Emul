#include "CodeGenerator.h"

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

CodeGenerator::FAnalysis CodeGenerator::BuildAnalysis(const std::vector<uint8_t>& Data, const std::vector<uint8_t>& DirtyMask, const FOptions& Options)
{
    if (Data.size() != ZX_SCREEN_SIZE)
    {
        throw std::runtime_error("BuildAnalysis: data must contain exactly 6912 bytes");
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
            throw std::runtime_error("BuildAnalysis: dirtyMask must contain exactly 6912 bytes");
        }

        Analysis.Dirty = DirtyMask;
    }

    Analysis.StartsAt.resize(ZX_SCREEN_SIZE);

    // ------------------------------------------------------------
    // 1. Одиночные байты.
    // ------------------------------------------------------------
    for (int32_t i = 0; i < ZX_SCREEN_SIZE; ++i)
    {
        if (!Analysis.Dirty[i])
        {
            continue;
        }

        AddCandidate(Analysis, MakeByteAbsA(Data, i));
        AddCandidate(Analysis, MakeByteHLImm(Data, i));
    }

    // ------------------------------------------------------------
    // 2. Обычные 2 байта через LD HL, Word / LD (nn), HL.
    // ------------------------------------------------------------
    for (int32_t i = 0; i + 1 < ZX_SCREEN_SIZE; ++i)
    {
        if (!RangeIsDirty(Analysis.Dirty, i, i + 2))
        {
            continue;
        }

        AddCandidate(Analysis, MakeWordAbsHL(Data, i));
    }

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
                AddCandidate(Analysis, MakeStackBlock(Data, Start, Pairs * 2));
            }
        }

        // Большой блок на весь непрерывный участок.
        if ((RangeLength & 1) == 0 && RangeLength / 2 > Options.MaxStackPairsToEnumerate)
        {
            AddCandidate(Analysis, MakeStackBlock(Data, RangeStart, RangeLength));
        }
    }

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
            AddCandidate(Analysis, MakeRepeatWordAbsHL(Data, Start, PairCount));
            AddCandidate(Analysis, MakeRepeatWordStack(Data, Start, PairCount));
        }
    }

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
            AddCandidate(Analysis, MakeHorizontalSameByteIncL(Data, Start, Length));
        }
    }

    return Analysis;
}

CodeGenerator::FPlan CodeGenerator::OptimizePlan(const FAnalysis& Analysis, const FOptions& Options)
{
    const int32_t N = ZX_SCREEN_SIZE;
    const int64_t INF = (std::numeric_limits<int64_t>::max)() / 4;

    std::vector<int64_t> DP(N + 1, INF);
    std::vector<int32_t> Choice(N + 1, -1);

    DP[N] = 0;

    for (int32_t i = N - 1; i >= 0; --i)
    {
        if (!Analysis.Dirty[i])
        {
            DP[i] = DP[i + 1];
            Choice[i] = -2; // skip
            continue;
        }

        for (int32_t ID : Analysis.StartsAt[i])
        {
            const FCandidate& Candidate = Analysis.Candidates[ID];
            if (!Candidate.Linear)
            {
                continue;
            }

            if (Candidate.EndOffset <= i || Candidate.EndOffset > N)
            {
                continue;
            }

            if (!RangeIsDirty(Analysis.Dirty, Candidate.StartOffset, Candidate.EndOffset))
            {
                continue;
            }

            if (DP[Candidate.EndOffset] == INF)
            {
                continue;
            }

            const int64_t Cost = ScoreCandidate(Candidate, Options) + DP[Candidate.EndOffset];
            if (Cost < DP[i])
            {
                DP[i] = Cost;
                Choice[i] = ID;
            }
        }

        // На всякий случай: если кандидатов не нашлось, пишем байт.
        if (Choice[i] == -1 && Analysis.Dirty[i])
        {
            FCandidate Fallback = MakeByteAbsA(Analysis.Data, i);
            const int64_t Cost = ScoreCandidate(Fallback, Options) + DP[i + 1];

            if (Cost < DP[i])
            {
                // Dallback не добавлен в Analysis, поэтому в норме сюда не попадём.
                // Оставлено как защита.
                DP[i] = Cost;
            }
        }
    }

    FPlan Plan;
    int32_t Pos = 0;

    while (Pos < N)
    {
        if (!Analysis.Dirty[Pos])
        {
            ++Pos;
            continue;
        }

        const int32_t ID = Choice[Pos];
        if (ID < 0)
        {
            throw std::runtime_error("OptimizePlan: failed to cover dirty byte");
        }

        const FCandidate& Candidate = Analysis.Candidates[ID];

        Plan.CandidateIds.push_back(ID);
        Plan.TotalCycles += Candidate.Cycles;
        Plan.TotalCodeBytes += Candidate.CodeBytes;

        Pos = Candidate.EndOffset;
    }

    return Plan;
}

void CodeGenerator::EmitLdA(std::ostringstream& Out, FEmitState& State, uint8_t Value)
{
    if (!State.bHasA || State.A != Value)
    {
        Out << "                LD A, " << Hex8(Value) << "\n";
        State.A = Value;
        State.bHasA = true;
    }
}

void CodeGenerator::EmitLdHL(std::ostringstream& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasHL || State.HL != Value)
    {
        Out << "                LD HL, " << Hex16(Value) << "\n";
        State.HL = Value;
        State.bHasHL = true;
    }
}

void CodeGenerator::EmitLdSP(std::ostringstream& Out, FEmitState& State, uint16_t Value)
{
    if (!State.bHasSP || State.SP != Value)
    {
        Out << "                LD SP, " << Hex16(Value) << "\n";
        State.SP = Value;
        State.bHasSP = true;
    }
}

void CodeGenerator::EmitCandidate(std::ostringstream& Out, const FCandidate& Candidate, const std::vector<uint8_t>& Data, FEmitState& State)
{
    switch (Candidate.Kind)
    {
    case EOpKind::ByteAbsA:
    {
        EmitLdA(Out, State, Candidate.ByteValue);
        Out << "                LD (" << Hex16(Candidate.StartAddr) << "), A\n";
        break;
    }

    case EOpKind::ByteHLImm:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);
        Out << "                LD (HL), " << Hex8(Candidate.ByteValue) << "\n";

        // HL не меняется.
        break;
    }

    case EOpKind::WordAbsHL:
    {
        EmitLdHL(Out, State, Candidate.WordValue);
        Out << "                LD (" << Hex16(Candidate.StartAddr) << "), HL\n";
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
            Out << "                PUSH HL\n";

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
            Out << "                LD (" << Hex16(AddrOf(Pos)) << "), HL\n";
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
            Out << "                PUSH HL\n";

            State.SP = static_cast<uint16_t>(State.SP - 2);
            State.bHasSP = true;
        }

        break;
    }

    case EOpKind::HorizontalSameByteIncL:
    {
        EmitLdHL(Out, State, Candidate.StartAddr);
        EmitLdA(Out, State, Candidate.ByteValue);

        Out << "                LD (HL), A\n";

        uint16_t HL = Candidate.StartAddr;
        for (int32_t i = 1; i < Candidate.WriteBytes; ++i)
        {
            Out << "                INC L\n";
            Out << "                LD (HL), A\n";
            HL = static_cast<uint16_t>((HL & 0xFF00) | ((HL + 1) & 0x00FF));
        }

        State.HL = HL;
        State.bHasHL = true;
        break;
    }

    default:
        throw std::runtime_error("EmitCandidate: unknown candidate kind");
    }
}

std::string CodeGenerator::EmitAsm(const FAnalysis& Analysis, const FPlan& Plan, const FOptions& Options, const std::string& LabelName)
{
    std::ostringstream Out;
    FEmitState State;

    Out << LabelName << ":\n";
    if (Options.DisableInterruptsForStack)
    {
        Out << "                DI\n";
    }

    if (Options.PreserveSP)
    {
        // Самомодификация: сохраняем текущий SP в операнд LD SP,#0000.
        // Код должен быть в RAM.
        Out << "                LD (" << LabelName << "_RestoreSP + 1), SP\n";
    }
    Out << "\n";

    for (int32_t ID : Plan.CandidateIds)
    {
        const FCandidate& Candidate = Analysis.Candidates[ID];
        Out << "                ; "
            << ToString(Candidate.Kind)
            << " addr=" << Hex16(Candidate.StartAddr)
            << " bytes=" << Candidate.WriteBytes
            << " cycles=" << Candidate.Cycles
            << " code=" << Candidate.CodeBytes
            << "\n";

        EmitCandidate(Out, Candidate, Analysis.Data, State);
        Out << "\n";
    }

    if (Options.PreserveSP)
    {
        Out << LabelName << "_RestoreSP:\n";
        Out << "                LD SP, #0000\n";
    }

    if (Options.DisableInterruptsForStack)
    {
        Out << "                EI\n";
    }

    Out << "                RET\n";

    return Out.str();
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
            ++HorizCount;
            break;
        }
    }

    std::cout << "Stack blocks: " << StackCount << "\n";
    std::cout << "Word writes: " << WordCount << "\n";
    std::cout << "Byte writes: " << ByteCount << "\n";
    std::cout << "Repeat blocks: " << RepeatCount << "\n";
    std::cout << "Horizontal runs: " << HorizCount << "\n";
}