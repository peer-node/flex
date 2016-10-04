#ifndef TELEPORT_INITIALDATAMESSAGE_H
#define TELEPORT_INITIALDATAMESSAGE_H


#include "InitialDataRequestMessage.h"
#include "EnclosedMessageData.h"

#include "log.h"
#define LOG_CATEGORY "InitialDataMessage.h"

class CreditSystem;
class Calend;

class InitialDataMessage
{
public:
    uint160 request_hash{0};
    BitChain spent_chain;
    std::vector<MinedCreditMessage> mined_credit_messages_in_current_diurn;
    EnclosedMessageData message_data;

    InitialDataMessage() { }

    InitialDataMessage(InitialDataRequestMessage request, CreditSystem *credit_system);

    void PopulateEnclosedData(uint160 msg_hash, CreditSystem *credit_system);

    void SetSpentChain(CreditSystem* credit_system);

    void PopulateMinedCreditMessages(uint160 msg_hash, CreditSystem *credit_system);

    void AddMinedCreditMessage(uint160 msg_hash, CreditSystem *credit_system);

    template <typename T>
    vch_t Serialize(T message)
    {
        CDataStream ss(SER_NETWORK, CLIENT_VERSION);
        ss << message;
        return vch_t(ss.begin(), ss.end());
    }

    static std::string Type() { return "initial_data"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(request_hash);
        READWRITE(spent_chain);
        READWRITE(mined_credit_messages_in_current_diurn);
        READWRITE(message_data);
    )

    DEPENDENCIES();

    HASH160();

    Calend GetLastCalend();
};


#endif //TELEPORT_INITIALDATAMESSAGE_H
