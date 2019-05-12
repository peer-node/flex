#ifndef TELEPORT_VECTOR_TOOLS_H
#define TELEPORT_VECTOR_TOOLS_H

#include <vector>
#include <array>
#include <algorithm>

template <typename T>
bool VectorContainsEntry(std::vector<T> vector_, T entry)
{
    return std::find(vector_.begin(), vector_.end(), entry) != vector_.end();
}

template <typename T, std::size_t N>
bool ArrayContainsEntry(std::array<T, N> array_, T entry)
{
    return std::find(array_.begin(), array_.end(), entry) != array_.end();
}

template <typename T>
int64_t PositionOfEntryInVector(T entry, std::vector<T> vector_)
{
    return std::distance(vector_.begin(), std::find(vector_.begin(), vector_.end(), entry));
}

template <typename T, std::size_t N>
int64_t PositionOfEntryInArray(T entry, std::array<T, N> array_)
{
    return std::distance(array_.begin(), std::find(array_.begin(), array_.end(), entry));
}

template <typename T>
void EraseEntryFromVector(T entry, std::vector<T>& vector_)
{
    int64_t position = PositionOfEntryInVector(entry, vector_);
    if (position < vector_.size())
        vector_.erase(vector_.begin() + position);
}

template <typename T>
std::vector<T> ConcatenateVectors(std::vector<T> &vector1, std::vector<T> &vector2)
{
    auto result = vector1;
    result.insert(result.end(), vector2.begin(), vector2.end());
    return result;
}


#endif //TELEPORT_VECTOR_TOOLS_H
