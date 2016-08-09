#ifndef FLEX_FLEXNETWORKNODE_H
#define FLEX_FLEXNETWORKNODE_H


#include "Calendar.h"
#include "Wallet.h"
#include "CreditMessageHandler.h"
#include "DataMessageHandler.h"

class FlexNetworkNode
{
public:
    MemoryDataStore msgdata, keydata, creditdata;
    Calendar calendar;
    Wallet *wallet;
    CreditMessageHandler *credit_message_handler;
    DataMessageHandler *data_message_handler;
    CreditSystem *credit_system;
    BitChain spent_chain;

    FlexNetworkNode()
    {
        credit_system = new CreditSystem(msgdata, creditdata);
        wallet = new Wallet(keydata);
        credit_message_handler = new CreditMessageHandler(msgdata, creditdata, keydata);
        credit_message_handler->SetCreditSystem(credit_system);
        credit_message_handler->SetCalendar(calendar);
        credit_message_handler->SetSpentChain(spent_chain);
        credit_message_handler->SetWallet(*wallet);

        data_message_handler = new DataMessageHandler(msgdata, creditdata);
        data_message_handler->SetCreditSystem(credit_system);
        data_message_handler->SetCalendar(&calendar);
    }

    ~FlexNetworkNode()
    {
        delete data_message_handler;
        delete credit_message_handler;
        delete credit_system;
        delete wallet;
    }

    Calendar GetCalendar();

    uint64_t Balance();

    void HandleMessage(std::string channel, CDataStream stream, CNode *peer);

    MinedCreditMessage Tip();

    MinedCreditMessage GenerateMinedCreditMessageWithoutProofOfWork();

    void HandleNewProof(NetworkSpecificProofOfWork proof);

    MinedCreditMessage RecallGeneratedMinedCreditMessage(uint256 credit_hash);

    uint160 SendCreditsToPublicKey(Point public_key, uint64_t amount);

    Point GetNewPublicKey();

    uint160 SendToAddress(std::string address, int64_t amount);

};


#endif //FLEX_FLEXNETWORKNODE_H
