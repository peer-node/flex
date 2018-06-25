
#ifndef TELEPORT_LISTEXPANSIONMESSAGE_H
#define TELEPORT_LISTEXPANSIONMESSAGE_H

#include "test/teleport_tests/node/credit/structures/CreditSystem.h"
#include "ListExpansionRequestMessage.h"

#include "log.h"
#define LOG_CATEGORY "ListExpansionMessage.h"

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
        MinedCreditMessage msg = credit_system->msgdata[request.mined_credit_message_hash]["msg"];
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
            if (type == "msg")
            {
                MinedCreditMessage msg = credit_system->msgdata[hash]["msg"];
                CDataStream ss(SER_NETWORK, CLIENT_VERSION);
                ss << msg;
                message_content = vch_t(ss.begin(), ss.end());
            }
            else if (type == "tx")
            {
                SignedTransaction tx = credit_system->msgdata[hash]["tx"];
                CDataStream ss(SER_NETWORK, CLIENT_VERSION);
                ss << tx;
                message_content = vch_t(ss.begin(), ss.end());
            }
            else if (type == "relay_join")
            {
                RelayJoinMessage join = credit_system->msgdata[hash]["relay_join"];
                CDataStream ss(SER_NETWORK, CLIENT_VERSION);
                ss << join;
                message_content = vch_t(ss.begin(), ss.end());
            }
            else
            {
                log_ << "No implemented procedure for message type " << type << "\n";
            }
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

    JSON(request_hash, message_types, message_contents);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_LISTEXPANSIONMESSAGE_H
