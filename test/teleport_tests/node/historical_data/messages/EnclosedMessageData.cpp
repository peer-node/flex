#include <test/teleport_tests/node/deposits/messages/DepositAddressRequest.h>
#include "EnclosedMessageData.h"
#include "test/teleport_tests/node/credit/structures/CreditSystem.h"

#include "log.h"
#define LOG_CATEGORY "EnclosedMessageData.cpp"

void EnclosedMessageData::PopulateMinedCreditMessagesAndEnclosedData(uint160 msg_hash, CreditSystem *credit_system)
{
    AddMinedCreditMessageAndContents(msg_hash, credit_system);
    msg_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
    while (msg_hash != 0)
    {
        AddMinedCreditMessageAndContents(msg_hash, credit_system);
        if (credit_system->IsCalend(msg_hash))
            break;
        msg_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
    }
}

void EnclosedMessageData::AddMinedCreditMessageAndContents(uint160 msg_hash, CreditSystem *credit_system)
{
    MinedCreditMessage msg = credit_system->msgdata[msg_hash]["msg"];
    AddMessagesContainedInMinedCreditMessage(msg, credit_system);
}

void EnclosedMessageData::AddMessagesContainedInMinedCreditMessage(MinedCreditMessage msg, CreditSystem *credit_system)
{
    msg.hash_list.RecoverFullHashes(credit_system->msgdata);
    for (auto hash : msg.hash_list.full_hashes)
    {
        std::string type = credit_system->MessageType(hash);
        log_ << "AddMessagesContainedInMinedCreditMessage: adding " <<  hash << " of type " << type << "\n";
        if (type == "tx")
        {
            AddTransaction(hash, credit_system);
        }
        else if (type == "msg" and credit_system->IsCalend(msg.GetHash160()))
            AddMinedCreditMessage(hash, credit_system);
        else if (type == "relay_join")
            AddRelayJoinMessage(hash, credit_system);
        else if (type == "deposit_request")
            AddDepositAddressRequest(hash, credit_system);
        else if (type != "msg")
        {
            log_ << "No implementation for message type " << type << "\n";
        }
    }
}

void EnclosedMessageData::AddTransaction(uint160 tx_hash, CreditSystem *credit_system)
{
    SignedTransaction tx = credit_system->msgdata[tx_hash]["tx"];
    enclosed_message_types.push_back("tx");
    enclosed_message_contents.push_back(Serialize(tx));
}

void EnclosedMessageData::AddMinedCreditMessage(uint160 msg_hash, CreditSystem *credit_system)
{
    MinedCreditMessage msg = credit_system->msgdata[msg_hash]["msg"];
    enclosed_message_types.push_back("msg");
    enclosed_message_contents.push_back(Serialize(msg));
}

void EnclosedMessageData::AddRelayJoinMessage(uint160 join_hash, CreditSystem *credit_system)
{
    RelayJoinMessage join = credit_system->msgdata[join_hash]["relay_join"];
    enclosed_message_types.push_back("relay_join");
    enclosed_message_contents.push_back(Serialize(join));
}

void EnclosedMessageData::AddDepositAddressRequest(uint160 request_hash, CreditSystem *credit_system)
{
    DepositAddressRequest request = credit_system->msgdata[request_hash]["deposit_request"];
    enclosed_message_types.push_back("deposit_request");
    enclosed_message_contents.push_back(Serialize(request));
}
