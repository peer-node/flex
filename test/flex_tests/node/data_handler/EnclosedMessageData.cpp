#include "EnclosedMessageData.h"
#include "test/flex_tests/node/CreditSystem.h"


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
        if (type == "tx")
        {
            AddTransaction(hash, credit_system);
        }
        if (type == "msg" and credit_system->IsCalend(msg.GetHash160()))
            AddMinedCreditMessage(hash, credit_system);
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
