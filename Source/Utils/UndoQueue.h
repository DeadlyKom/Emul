#pragma once

#include <CoreMinimal.h>

namespace Undo
{
    class IAction
    {
    public:
        virtual void Execute() = 0;
        virtual void Unite(void* A, const void* B) = 0;
        virtual ~IAction() {}
    };

    using FActionPtr = std::shared_ptr<IAction>;

    template<typename TObject, typename TArg>
    class TAction : public IAction
    {
    public:
        template<typename TCallableA, typename TCallableB>
        TAction(TCallableA InFunc, TCallableB InUniteFunc, TArg InParam)
            : Func(std::forward<TCallableA>(InFunc))
            , UniteFunc(std::forward<TCallableB>(InUniteFunc))
            , Param(std::move(InParam))
        {}

        virtual void Execute() override
        {
            Func(Param);
        }
        virtual void Unite(void* _A, const void* _B) override
        {
            auto A_ptr = static_cast<IAction*>(_A);
            auto B_ptr = static_cast<const IAction*>(_B);

            auto A_action = dynamic_cast<TAction<TObject, TArg>*>(A_ptr);
            auto B_action = dynamic_cast<const TAction<TObject, TArg>*>(B_ptr);
            UniteFunc(A_action->Param, B_action->Param);
        }

    private:
        std::function<void(TArg&)> Func;
        std::function<void(TArg&, const TArg&)> UniteFunc;
        TArg Param;
    };

    class FQueue
    {
    public:
        void SetWithUndo(FActionPtr Action);
        void Stroke(size_t StrokeNum);
        void Undo();
        void Redo();
        void Clear();

        void BeginContinuous();
        void EndContinuous();
        bool IsContinuous() const { return bIsContinuous; }
        size_t UndoSize() const { return UndoStack.size(); }
        std::stack<FActionPtr>& GetUndoStack() { return UndoStack; }

    private:
        std::stack<FActionPtr> UndoStack;
        std::stack<FActionPtr> RedoStack;

        bool bIsContinuous = false;
        FActionPtr CurrentContinuous;
    };
}
