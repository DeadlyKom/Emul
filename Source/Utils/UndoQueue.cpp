#include "UndoQueue.h"

namespace Undo
{
    void FQueue::SetWithUndo(FActionPtr Action)
    {
        Action->Execute();
        UndoStack.push(Action);
        RedoStack = {};
    }

    void FQueue::Undo()
    {
        if (UndoStack.empty())
        {
            return;
        }

        while (!UndoStack.empty())
        {
            FActionPtr Action = UndoStack.top();
            UndoStack.pop();
            Action->Execute();
            RedoStack.push(Action);

            if (dynamic_cast<FContinuousMarkerAction*>(Action.get()))
            {
                break;
            }
        }
    }

    void FQueue::Redo()
    {
        if (RedoStack.empty())
        {
            return;
        }
        while (!RedoStack.empty())
        {
            FActionPtr Action = RedoStack.top();
            RedoStack.pop();
            Action->Execute();
            UndoStack.push(Action);

            if (dynamic_cast<FContinuousMarkerAction*>(Action.get()))
            {
                break;
            }
        }
    }

    void FQueue::Clear()
    {
        UndoStack = {};
        RedoStack = {};
    }
    void FQueue::BeginContinuous()
    {
        bIsContinuous = true;
        UndoStack.push(std::make_shared<FContinuousMarkerAction>());
    }
    void FQueue::EndContinuous()
    {
        bIsContinuous = false;
    }
}