
#ifndef FLEX_LISTEXPANSIONMESSAGE_H
#define FLEX_LISTEXPANSIONMESSAGE_H

#include "CreditSystem.h"
#include "ListExpansionRequestMessage.h"

class ListExpansionMessage
{
public:
    uint160 request_hash;
    std::vector<std::string> message_types;
    std::vector<vch_t> message_contents;

    ListExpansionMessage() { }

    ListExpansionMessage(ListExpansionRequestMessage request, CreditSystem *credit_system)
    {
        request_hash = request.GetHash160();
        MinedCreditMessage msg = credit_system->creditdata[request.mined_credit_hash]["msg"];
        msg.hash_list.RecoverFullHashes(credit_system->msgdata);
        PopulateMessages(msg.hash_list.full_hashes, credit_system);
    }

    void PopulateMessages(std::vector<uint160> message_hashes, CreditSystem *credit_system)
    {
        for (auto hash : message_hashes)
        {
            std::string type = credit_system->MessageType(hash);
            message_types.push_back(type);
            vch_t message_content;
            if (type == "mined_credit")
            {
                MinedCreditMessage msg = credit_system->creditdata[hash]["msg"];
                CDataStream ss(SER_NETWORK, CLIENT_VERSION);
                ss << msg;
                message_content = vch_t(ss.begin(), ss.end());
            }
            else
                message_content = credit_system->msgdata[hash][type].serialized_value;
            message_contents.push_back(message_content);
        }
    }

    static std::string Type() { return "list_expansion"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(request_hash);
        READWRITE(message_types);
        READWRITE(message_contents);
    )

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_LISTEXPANSIONMESSAGE_H
