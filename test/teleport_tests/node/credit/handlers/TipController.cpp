#include <src/vector_tools.h>
#include <test/teleport_tests/node/calendar/Calendar.h>
#include "TipController.h"
#include "MinedCreditMessageBuilder.h"

#include "log.h"
#define LOG_CATEGORY "TipController.cpp"

using std::vector;
using std::set;


void TipController::SetMinedCreditMessageBuilder(MinedCreditMessageBuilder *builder_) { builder = builder_; }

void TipController::SetCreditSystem(CreditSystem *credit_system_) { credit_system = credit_system_; }

void TipController::SetCalendar(Calendar *calendar_) { calendar = calendar_; }

void TipController::SetSpentChain(BitChain *spent_chain_) { spent_chain = spent_chain_; }

void TipController::SetWallet(Wallet *wallet_) { wallet = wallet_; }

MinedCreditMessage TipController::Tip()
{
    return calendar->LastMinedCreditMessage();
}

void TipController::SwitchToNewTipIfAppropriate()
{
    vector<uint160> batches_with_the_most_work = credit_system->MostWorkBatches();

    uint160 current_tip = calendar->LastMinedCreditMessageHash();
    if (VectorContainsEntry(batches_with_the_most_work, current_tip))
    {
        log_ << "============= Not switching - current tip " << current_tip << " has more work\n";
        log_ << current_tip << " has work " << calendar->LastMinedCreditMessage().mined_credit.ReportedWork() << "\n";
        return;
    }

    if (batches_with_the_most_work.size() == 0)
    {
        log_ << "============= Not switching - zero batches have the most work\n";
        return;
    }

    uint160 new_tip = batches_with_the_most_work[0];
    SwitchToTip(new_tip);
}

void TipController::SwitchToTip(uint160 msg_hash_of_new_tip)
{
    log_ << "switching to new tip: " << msg_hash_of_new_tip << "\n";
    MinedCreditMessage msg_at_new_tip = msgdata[msg_hash_of_new_tip]["msg"];
    uint160 current_tip = calendar->LastMinedCreditMessageHash();

    if (msg_at_new_tip.mined_credit.network_state.previous_mined_credit_message_hash == current_tip)
        AddToTip(msg_at_new_tip);
    else
        SwitchToTipViaFork(msg_hash_of_new_tip);
}

void TipController::SwitchToTipViaFork(uint160 new_tip)
{
    log_ << "switching via fork\n";
    uint160 old_tip = calendar->LastMinedCreditMessageHash();
    uint160 current_tip = calendar->LastMinedCreditMessageHash();
    *spent_chain = credit_system->GetSpentChainOnOtherProngOfFork(*spent_chain, current_tip, new_tip);
    credit_system->SwitchMainChainToOtherBranchOfFork(current_tip, new_tip);
    *calendar = Calendar(new_tip, credit_system);
    builder->UpdateAcceptedMessagesAfterFork(old_tip, new_tip);
    if (wallet != NULL)
        wallet->SwitchAcrossFork(old_tip, new_tip, credit_system);
}

void TipController::AddToTip(MinedCreditMessage &msg)
{
    log_ << "adding tip to end of current chain\n";
    credit_system->StoreMinedCreditMessage(msg);
    log_ << "storing mined credit message " << msg.GetHash160() << "\n";
    credit_system->AddToMainChain(msg);
    *spent_chain = credit_system->GetSpentChainOnOtherProngOfFork(*spent_chain,
                                                                  calendar->LastMinedCreditMessageHash(),
                                                                  msg.GetHash160());
    log_ << "====  getting calendar to add to tip. previous calendar tip was: " << calendar->LastMinedCreditMessageHash() << "\n";
    calendar->AddToTip(msg, credit_system);
    log_ << "====  new calendar tip is: " << calendar->LastMinedCreditMessageHash() << "\n";
    if (wallet != NULL)
        wallet->AddBatchToTip(msg, credit_system);
    log_ << "updating accepted messages after new tip\n";
    builder->UpdateAcceptedMessagesAfterNewTip(msg);

    log_ << "tracking credits\n";
    auto batch = credit_system->ReconstructBatch(msg);
    credit_system->tracker.StoreRecipientsOfCreditsInBatch(batch);
}

void TipController::RemoveFromMainChainAndSwitchToNewTip(uint160 msg_hash)
{
    credit_system->RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(msg_hash);
    SwitchToNewTipIfAppropriate();
}
