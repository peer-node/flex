#ifndef TELEPORT_TIPCONTROLLER_H
#define TELEPORT_TIPCONTROLLER_H


#include <test/teleport_tests/node/wallet/Wallet.h>
#include <test/teleport_tests/node/Data.h>
#include "test/teleport_tests/node/credit_handler/messages/MinedCreditMessage.h"

class MinedCreditMessageBuilder;

class TipController
{
public:
    MemoryDataStore &msgdata, &creditdata, &keydata;
    CreditSystem *credit_system;
    Calendar *calendar;
    Mutex calendar_mutex;
    BitChain *spent_chain;
    Wallet *wallet{NULL};
    MinedCreditMessageBuilder *builder;

    TipController(Data data):
            msgdata(data.msgdata), creditdata(data.creditdata), keydata(data.keydata)
    { }

    void SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder_);

    void SetCreditSystem(CreditSystem *credit_system_);

    void SetCalendar(Calendar *calendar_);

    void SetSpentChain(BitChain *spent_chain_);

    void SetWallet(Wallet *wallet);

    void AddToTip(MinedCreditMessage &msg);

    void RemoveFromMainChainAndSwitchToNewTip(uint160 credit_hash);

    void SwitchToNewTipIfAppropriate();

    void SwitchToTip(uint160 credit_hash);

    void SwitchToTipViaFork(uint160 credit_hash_of_new_tip);

    MinedCreditMessage Tip();
};


#endif //TELEPORT_TIPCONTROLLER_H
