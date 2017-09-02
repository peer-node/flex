#ifndef TELEPORT_MINEDCREDITMESSAGEBUILDER_H
#define TELEPORT_MINEDCREDITMESSAGEBUILDER_H


#include <test/teleport_tests/node/credit/structures/CreditSystem.h>
#include <src/crypto/hashtrees.h>
#include <test/teleport_tests/node/wallet/Wallet.h>
#include <test/teleport_tests/node/config/TeleportConfig.h>
#include <test/teleport_tests/node/Data.h>


class MinedCreditMessageBuilder
{
public:
    TeleportConfig config;
    Data data;
    MemoryDataStore &msgdata, &creditdata, &keydata;
    CreditSystem *credit_system;
    Calendar *calendar;
    BitChain *spent_chain;
    Wallet *wallet{NULL};
    std::vector<uint160> accepted_messages;

    MinedCreditMessageBuilder(Data data_):
            data(data_), msgdata(data.msgdata), creditdata(data.creditdata), keydata(data.keydata)
    { }

    void SetConfig(TeleportConfig& config_);

    void SetCreditSystem(CreditSystem *credit_system_);

    void SetCalendar(Calendar *calendar_);

    void SetSpentChain(BitChain *spent_chain_);

    void SetWallet(Wallet *wallet_) ;

    void AddToAcceptedMessages(uint160 message_hash);

    void UpdateAcceptedMessagesAfterNewTip(MinedCreditMessage &msg);

    MinedCreditMessage Tip();

    void UpdateAcceptedMessagesAfterFork(uint160 old_tip, uint160 new_tip);

    void UpdateAcceptedMessagesAfterFork(std::vector<uint160> messages_on_old_branch,
                                         std::vector<uint160> messages_on_new_branch);

    std::set<uint64_t> PositionsSpentByAcceptedTransactions();

    void ValidateAcceptedMessagesAfterFork();

    bool TransactionHasNoSpentInputs(SignedTransaction tx, std::set<uint160> &spent_positions);

    MinedCreditMessage GenerateMinedCreditMessageWithoutProofOfWork();

    void SetRelayStateHashFromPreviousStateHashAndEnclosedFullHashes(MinedCreditMessage &msg);
};


#endif //TELEPORT_MINEDCREDITMESSAGEBUILDER_H
