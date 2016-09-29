#ifndef FLEX_INITIALDATAMESSAGE_H
#define FLEX_INITIALDATAMESSAGE_H


#include "test/flex_tests/node/CreditSystem.h"
#include "InitialDataRequestMessage.h"
#include "EnclosedMessageData.h"

#include "log.h"
#define LOG_CATEGORY "InitialDataMessage.h"



class InitialDataMessage
{
public:
    uint160 request_hash{0};
    BitChain spent_chain;
    std::vector<MinedCreditMessage> mined_credit_messages_in_current_diurn;
    EnclosedMessageData message_data;

    InitialDataMessage() { }

    InitialDataMessage(InitialDataRequestMessage request, CreditSystem *credit_system)
    {
        request_hash = request.GetHash160();
        PopulateMinedCreditMessages(request.latest_mined_credit_hash, credit_system);
        PopulateEnclosedData(request.latest_mined_credit_hash, credit_system);
        SetSpentChain(credit_system);
    }

    void PopulateEnclosedData(uint160 msg_hash, CreditSystem *credit_system)
    {
        message_data = EnclosedMessageData(msg_hash, credit_system);
    }

    void SetSpentChain(CreditSystem* credit_system)
    {
        Calend last_calend = GetLastCalend();
        spent_chain = credit_system->GetSpentChain(last_calend.GetHash160());
    }

    void PopulateMinedCreditMessages(uint160 msg_hash, CreditSystem *credit_system)
    {
        AddMinedCreditMessage(msg_hash, credit_system);
        msg_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
        while (msg_hash != 0)
        {
            AddMinedCreditMessage(msg_hash, credit_system);
            if (credit_system->IsCalend(msg_hash))
                break;
            msg_hash = credit_system->PreviousMinedCreditMessageHash(msg_hash);
        }
        std::reverse(mined_credit_messages_in_current_diurn.begin(), mined_credit_messages_in_current_diurn.end());
    }

    void AddMinedCreditMessage(uint160 msg_hash, CreditSystem *credit_system)
    {
        MinedCreditMessage msg = credit_system->msgdata[msg_hash]["msg"];
        mined_credit_messages_in_current_diurn.push_back(msg);
    }

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

    Calend GetLastCalend()
    {
        if (mined_credit_messages_in_current_diurn.size() == 0)
            return Calend();
        return mined_credit_messages_in_current_diurn[0];
    }
};


#endif //FLEX_INITIALDATAMESSAGE_H
