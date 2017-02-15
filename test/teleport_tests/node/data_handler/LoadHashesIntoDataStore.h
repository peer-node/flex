#ifndef TELEPORT_LOADHASHESINTODATASTORE_H
#define TELEPORT_LOADHASHESINTODATASTORE_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <test/teleport_tests/node/CreditSystem.h>


void LoadHashesIntoDataStoreFromMessageTypesAndContents(MemoryDataStore &hashdata,
                                                        std::vector<std::string> &types,
                                                        std::vector<vch_t> &contents,
                                                        CreditSystem *credit_system);


#endif //TELEPORT_LOADHASHESINTODATASTORE_H
