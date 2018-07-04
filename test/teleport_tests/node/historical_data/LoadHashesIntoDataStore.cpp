#include "LoadHashesIntoDataStore.h"

#include "log.h"
#define LOG_CATEGORY "LoadHashesIntoDataStore.cpp"

void LoadHashesIntoDataStoreFromMessageTypesAndContents(MemoryDataStore &hashdata,
                                                        std::vector<std::string> &types,
                                                        std::vector<vch_t> &contents,
                                                        CreditSystem *credit_system)
{
    for (uint32_t i = 0; i < types.size(); i++)
    {
        uint160 enclosed_message_hash;
        if (types[i] != "msg")
            enclosed_message_hash = Hash160(contents[i].begin(), contents[i].end());
        else
        {
            CDataStream ss(contents[i], SER_NETWORK, CLIENT_VERSION);
            MinedCreditMessage msg;
            ss >> msg;
            enclosed_message_hash = msg.GetHash160();
        }
        log_ << "LoadHashesIntoDataStoreFromMessageTypesAndContents: storing " << enclosed_message_hash << "\n";
        credit_system->StoreHash(enclosed_message_hash, hashdata);
    }
}
