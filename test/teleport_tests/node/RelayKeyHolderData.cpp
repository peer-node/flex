#include "RelayKeyHolderData.h"



void RelayKeyHolderData::Retrieve(uint160 holder_data_hash, MemoryObject &object)
{
    *this = object[holder_data_hash];
}

void RelayKeyHolderData::Store(MemoryObject &object)
{
    object[GetHash160()] = *this;
}