// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef FLEX_CREDITNODE
#define FLEX_CREDITNODE

#include "initialize/dataserver.h"
#include "initialize/downloader.h"
#include "flexnode/eventnotifier.h"
#include "flexnode/schedule.h"
#include "flexnode/calendar.h"
#include "relays/relays.h"
#include "flexnode/wallet.h"
#include "flexnode/batchchainer.h"
#include "deposit/deposits.h"
#include "trade/trade.h"
#include "mining/pit.h"

#include "log.h"
#define LOG_CATEGORY "flexnode.h"

#ifndef TEST_BUILD

class FlexNode
{
public:
    uint160 previous_mined_credit_hash;

    DepositHandler deposit_handler;
    EventNotifier event_notifier;
    TradeHandler tradehandler;
    RelayHandler relayhandler;
    DataServer dataserver;
    Downloader downloader;
    BitChain spent_chain;
    BatchChainer chainer;
    Scheduler scheduler;
    Calendar calendar;
    Wallet wallet;
    CreditPit pit;
    std::map<vch_t,Currency> currencies;
    uint8_t digging;

    Mutex mutex;

    FlexNode():
        previous_mined_credit_hash(0)
    { }

    void ClearEncryptionKeys()
    {
        walletdata.ClearEncryptionKey();
        keydata.ClearEncryptionKey();
    }

    void InitializeWalletAndEncryptedDatabases(string_t passphrase)
    {
        uint256 passphrase_hash = TwistPasswordHash(vch_t(passphrase.begin(),
                                                          passphrase.end()));

        vch_t database_key(UBEGIN(passphrase_hash), UEND(passphrase_hash));
        keydata.SetEncryptionKey(database_key);
        walletdata.SetEncryptionKey(database_key);
        wallet = Wallet(passphrase_hash);
    }

    void Initialize()
    {
        InitializeRelayTasks();
        InitializeTradeTasks();
        downloader.Initialize();
        
        string_t passphrase;
        if (mapArgs.count("-walletpassword"))
        {
            passphrase = mapArgs["-walletpassword"];
        }
        else
        {
            printf("No passphrase found.\n"
                   "You should set the value of 'walletpassword' "
                   "in the configuration file.\n");
            passphrase = string_t("no passphrase");
        }

        InitializeWalletAndEncryptedDatabases(passphrase);

#ifndef _PRODUCTION_BUILD
        string_t warning("Warning: This is not a production build."
                         " Secrets will be written to the logs. Recompile "
                         "with -D_PRODUCTION_BUILD for secure trading.\n");
        log_ << warning;
        fprintf(stderr, "%s", warning.c_str());
#endif
        pit.Initialize();
        ClearMainChain();
        Test();
        log_ << "initializing currencies\n";
        InitializeCryptoCurrencies();
        log_ << "finished initializing currencies\n";
        InitializeDepositScheduledTasks();
        initdata["instance"]["start_time"] = GetTimeMicros();
    }

    void Test();

    void HandleTransaction(SignedTransaction tx, CNode *peer)
    {
        LOCK(mutex);
        chainer.HandleTransaction(tx, peer);
    }

    void HandleMinedCreditMessage(MinedCreditMessage &msg, CNode *peer)
    {
        LOCK(mutex);
        chainer.HandleMinedCreditMessage(msg, peer);
    }

    void HandleBadBlockMessage(BadBlockMessage &msg, CNode *peer)
    {
        LOCK(mutex);
        chainer.HandleBadBlockMessage(msg, peer);
    }

    void AddBatchToTip(MinedCreditMessage& msg)
    {
        log_ << "AddBatchToTip: " << msg.mined_credit.GetHash160() << "\n";
        LOCK(mutex);
        bool was_digging = digging;
        digging = false;

        chainer.AddBatchToTip(msg);
        uint160 new_mined_credit_hash = msg.mined_credit.GetHash160();

        ChangeSpentChain(&spent_chain, 
                         previous_mined_credit_hash, 
                         new_mined_credit_hash);

        AddToMainChain(new_mined_credit_hash);

        previous_mined_credit_hash = new_mined_credit_hash;
        calendar.AddMinedCreditToTip(msg);
        pit.AddBatchToTip(msg);
        relayhandler.AddBatchToTip(msg);
        deposit_handler.AddBatchToTip(msg);
        event_notifier.RecordEvent(new_mined_credit_hash);

        log_ << "AddBatchToTip(): finished.\n";
        digging = was_digging;
    }

    void RemoveBatchFromTip()
    {
        log_ << "RemoveBatchFromTip: " << previous_mined_credit_hash << "\n";
        LOCK(mutex);
        bool was_digging = digging;
        digging = false;

        uint160 preceding_credit_hash 
            = PreviousCreditHash(previous_mined_credit_hash);
        ChangeSpentChain(&spent_chain, 
                         previous_mined_credit_hash, 
                         preceding_credit_hash);

        RemoveFromMainChain(previous_mined_credit_hash);

        calendar.RemoveLast();
        chainer.RemoveBatchFromTip();
        pit.ResetBatch();
        relayhandler.UnHandleBatch();
        deposit_handler.RemoveBatchFromTip();
        previous_mined_credit_hash = preceding_credit_hash;
        
        digging = was_digging;
        log_ << "RemoveBatchFromTip(): finished.\n";
    }

    void InitiateChainAtCalend(uint160 calend_hash)
    {
        LOCK(mutex);
        bool was_digging = digging;
        digging = false;
        AddToMainChain(calend_hash);
        previous_mined_credit_hash = calend_hash;
        spent_chain = GetSpentChain(calend_hash);
        calendar = Calendar(calend_hash);
        pit.SwitchToNewCalend(calend_hash);
        ::RelayState state = GetRelayState(calend_hash);
        relayhandler = RelayHandler(state);
        deposit_handler.InitiateAtCalend(calend_hash);
        digging = was_digging;
    }

    ::RelayState& RelayState()
    {
        return relayhandler.relay_chain.state;
    }
};

#else

#include "test/test_flexnode.h"

#endif

extern FlexNode flexnode;

#endif
