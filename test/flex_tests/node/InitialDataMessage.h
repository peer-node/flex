#ifndef FLEX_INITIALDATAMESSAGE_H
#define FLEX_INITIALDATAMESSAGE_H


#include "CreditSystem.h"
#include "InitialDataRequestMessage.h"

#include "log.h"
#define LOG_CATEGORY "InitialDataMessage.h"

class InitialDataMessage
{
public:
    uint160 request_hash{0};
    BitChain spent_chain;
    std::vector<MinedCreditMessage> mined_credit_messages_in_current_diurn;
    std::vector<std::string> enclosed_message_types;
    std::vector<vch_t> enclosed_message_contents;

    InitialDataMessage() { }

    InitialDataMessage(InitialDataRequestMessage request, CreditSystem *credit_system)
    {
        request_hash = request.GetHash160();
        PopulateMinedCreditMessagesAndEnclosedData(request.latest_mined_credit_hash, credit_system);
        SetSpentChain(credit_system);
    }

    void SetSpentChain(CreditSystem* credit_system)
    {
        Calend last_calend = GetLastCalend();
        spent_chain = credit_system->GetSpentChain(last_calend.mined_credit.GetHash160());
    }

    void PopulateMinedCreditMessagesAndEnclosedData(uint160 credit_hash, CreditSystem *credit_system)
    {
        AddMinedCreditMessage(credit_hash, credit_system);
        credit_hash = credit_system->PreviousCreditHash(credit_hash);
        while (credit_hash != 0)
        {
            AddMinedCreditMessage(credit_hash, credit_system);
            if (credit_system->IsCalend(credit_hash))
                break;
            credit_hash = credit_system->PreviousCreditHash(credit_hash);
        }
        std::reverse(mined_credit_messages_in_current_diurn.begin(), mined_credit_messages_in_current_diurn.end());
    }

    void AddMinedCreditMessage(uint160 credit_hash, CreditSystem *credit_system)
    {
        MinedCreditMessage msg = credit_system->creditdata[credit_hash]["msg"];
        mined_credit_messages_in_current_diurn.push_back(msg);
        AddMessagesContainedInMinedCreditMessage(msg, credit_system);
    }

    void AddMessagesContainedInMinedCreditMessage(MinedCreditMessage msg, CreditSystem *credit_system)
    {
        msg.hash_list.RecoverFullHashes(credit_system->msgdata);
        for (auto hash : msg.hash_list.full_hashes)
        {
            std::string type = credit_system->MessageType(hash);
            if (type == "tx")
                AddTransaction(hash, credit_system);
            if (type == "mined_credit" and credit_system->IsCalend(msg.mined_credit.GetHash160()))
                AddMinedCredit(hash, credit_system);
        }
    }

    void AddTransaction(uint160 tx_hash, CreditSystem *credit_system)
    {
        SignedTransaction tx = credit_system->msgdata[tx_hash]["tx"];
        enclosed_message_types.push_back("tx");
        enclosed_message_contents.push_back(Serialize(tx));
    }

    void AddMinedCredit(uint160 credit_hash, CreditSystem *credit_system)
    {
        MinedCredit mined_credit = credit_system->creditdata[credit_hash]["mined_credit"];
        enclosed_message_types.push_back("mined_credit");
        enclosed_message_contents.push_back(Serialize(mined_credit));
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
        READWRITE(enclosed_message_types);
        READWRITE(enclosed_message_contents);
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
