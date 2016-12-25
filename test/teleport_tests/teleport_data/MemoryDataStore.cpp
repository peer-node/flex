#include "MemoryDataStore.h"

void MemoryDataStore::Reset()
{
    objects = std::map<vch_t, MemoryObject>();
}
