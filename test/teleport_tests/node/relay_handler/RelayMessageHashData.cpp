#include "RelayMessageHashData.h"


void RelayMessageHashData::Store(MemoryObject &object)
{
    object[GetHash160()] = *this;
}

void RelayMessageHashData::Retrieve(uint160 data_hash, MemoryObject &object)
{
    *this = object[data_hash];
}
