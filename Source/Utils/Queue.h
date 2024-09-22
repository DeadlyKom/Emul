#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

// https://www.geeksforgeeks.org/implement-thread-safe-queue-in-c/
template <typename T>
class TQueue
{
public:
    void Push(T Item)
    {
        std::unique_lock<std::mutex> Lock(Mutex);
        Queue.push(Item);
        Condition.notify_one();
    }

    T Pop()
    {
        std::unique_lock<std::mutex> Lock(Mutex);

        Condition.wait(Lock,
        [this]()
        { 
            return !Queue.empty();
        });

        T Item = Queue.front();
        Queue.pop();

        return Item;
    }

    bool IsEmpty()
    {
        std::unique_lock<std::mutex> Lock(Mutex);
        return Queue.empty();
    }

private:
    std::queue<T> Queue;
    std::mutex Mutex;
    std::condition_variable Condition;
};
