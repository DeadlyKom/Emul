#include "UndoQueue.h"

namespace Undo
{
    void FQueue::SetWithUndo(FActionPtr Action)
    {
        //if (bIsContinuous)
        //{
        //    if (!CurrentContinuous)
        //    {
        //        CurrentContinuous = Action;
        //        UndoStack.push(Action);
        //        RedoStack = {};
        //    }
        //    Action->Execute();
        //}
        //else
        {
            Action->Execute();
            UndoStack.push(Action);
            RedoStack = {};
        }
    }

    void FQueue::Stroke(size_t StrokeNum)
    {
        FActionPtr UnitedAction = UndoStack.top();
        UndoStack.pop();

        for (size_t Num = StrokeNum; Num > 1; --Num)
        {
            FActionPtr CurrentAction = UndoStack.top();
            UndoStack.pop();
            CurrentAction->Unite(CurrentAction.get(), UnitedAction.get());
            UnitedAction = CurrentAction;
        }
        UndoStack.push(UnitedAction);
    }

    void FQueue::Undo()
    {
        if (UndoStack.empty())
        {
            return;
        }
        FActionPtr Action = UndoStack.top();
        UndoStack.pop();
        Action->Execute();
        RedoStack.push(Action);
    }

    void FQueue::Redo()
    {
        if (RedoStack.empty())
        {
            return;
        }
        FActionPtr Action = RedoStack.top();
        RedoStack.pop();
        Action->Execute();
        UndoStack.push(Action);
    }

    void FQueue::Clear()
    {
        UndoStack = {};
        RedoStack = {};
    }
    void FQueue::BeginContinuous()
    {
        bIsContinuous = true;
    }
    void FQueue::EndContinuous()
    {
        bIsContinuous = false;
        CurrentContinuous.reset();
    }
}