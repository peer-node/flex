#include "MemoryLocationIterator.h"

void MemoryLocationIterator::SeekEnd()
{
    internal_iterator = end;
}

void MemoryLocationIterator::SeekStart()
{
    internal_iterator = start;
}
