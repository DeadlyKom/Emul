#pragma once

#include <CoreMinimal.h>

namespace Undo
{
    class IAction
    {
    public:
        virtual void Execute() = 0;
        virtual ~IAction() {}
    };

    using FActionPtr = std::shared_ptr<IAction>;

    class FContinuousMarkerAction : public IAction
    {
    public:
        void Execute() override {}
    };

    template<typename TArg>
    class TAction : public IAction
    {
    public:

        template<typename TCallable, typename TParam>
        TAction(TCallable&& InFunc, TParam InParam)
            : Func(std::forward<TCallable>(InFunc))
            , Param(std::move(InParam))
        {}

        virtual void Execute() override
        {
            Func(Param);
        }

    private:
        std::function<void(TArg&)> Func;
        TArg Param;
    };

    class FQueue
    {
    public:
        void SetWithUndo(FActionPtr Action);
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
    };
}
