#ifndef TELEPORT_ENCLOSEDMESSAGEDATA_H
#define TELEPORT_ENCLOSEDMESSAGEDATA_H


#include "crypto/uint256.h"
#include "test/teleport_tests/node/MinedCreditMessage.h"

#include "log.h"
#define LOG_CATEGORY "EnclosedMessageData.h"

class CreditSystem;


class EnclosedMessageData
{
public:
    std::vector<std::string> enclosed_message_types;
    std::vector<vch_t> enclosed_message_contents;

    EnclosedMessageData() { }

    EnclosedMessageData(uint160 msg_hash, CreditSystem *credit_system)
    {
        PopulateMinedCreditMessagesAndEnclosedData(msg_hash, credit_system);
    }

    void PopulateMinedCreditMessagesAndEnclosedData(uint160 msg_hash, CreditSystem *credit_system);

    void AddMinedCreditMessageAndContents(uint160 msg_hash, CreditSystem *credit_system);

    void AddMessagesContainedInMinedCreditMessage(MinedCreditMessage msg, CreditSystem *credit_system);

    void AddTransaction(uint160 tx_hash, CreditSystem *credit_system);

    void AddMinedCreditMessage(uint160 msg_hash, CreditSystem *credit_system);

    template <typename T>
    vch_t Serialize(T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << message;
        return vch_t(ss.begin(), ss.end());
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(enclosed_message_types);
        READWRITE(enclosed_message_contents);
    )

    JSON(enclosed_message_types, enclosed_message_contents);
};


#endif //TELEPORT_ENCLOSEDMESSAGEDATA_H
