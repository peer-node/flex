#include "MockDataStore.h"

void MockDataStore::Reset()
{
    objects = std::map<vch_t, MockObject>();
}
