#pragma once

template<typename T>
bool RemoveAtSwap(std::vector<T>& V, size_t I)
{
    const size_t L = V.size() - 1;
    if (I < V.size())
    {
        if (I != L)
        {
            std::swap(V[I], V[L]);
        }
        V.pop_back();

        return true;
    }
    return false;
}
