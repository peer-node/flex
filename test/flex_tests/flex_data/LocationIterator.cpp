#include "LocationIterator.h"

void LocationIterator::SeekEnd()
{
    internal_iterator = end;
}

void LocationIterator::SeekStart()
{
    internal_iterator = start;
}
