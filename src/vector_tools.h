#ifndef TELEPORT_VECTOR_TOOLS_H
#define TELEPORT_VECTOR_TOOLS_H

#include <vector>
#include <algorithm>

template <typename T>
bool VectorContainsEntry(std::vector<T> vector_, T entry)
{
    return std::find(vector_.begin(), vector_.end(), entry) != vector_.end();
}

template <typename T>
void EraseEntryFromVector(T entry, std::vector<T>& vector_)
{
    int64_t position = std::distance(vector_.begin(),
                            std::find(vector_.begin(), vector_.end(), entry));
    if (position < vector_.size())
    {
        vector_.erase(vector_.begin() + position);
    }
}


#endif //TELEPORT_VECTOR_TOOLS_H
