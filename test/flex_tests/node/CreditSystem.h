#ifndef FLEX_CREDITSYSTEM_H
#define FLEX_CREDITSYSTEM_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include <src/crypto/uint256.h>
#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <src/credits/SignedTransaction.h>
#include <src/credits/CreditBatch.h>
#include "MinedCreditMessage.h"
#include "Calend.h"
#include "CalendarFailureMessage.h"
#include "Calendar.h"


#define TARGET_BATCH_INTERVAL 60000000ULL // one minute
#define TARGET_DIURN_LENGTH (24 * 60 * 60 * 1000000ULL) // one day

#define MEMORY_FACTOR 19

uint160 AdjustDifficultyAfterBatchInterval(uint160 earlier_difficulty, uint64_t interval);
uint160 AdjustDiurnalDifficultyAfterDiurnDuration(uint160 earlier_diurnal_difficulty, uint64_t duration);

class CalendarMessage;

class CreditSystem
{
public:
    MemoryDataStore &msgdata, &creditdata;
    uint160 initial_difficulty{INITIAL_DIFFICULTY};
    uint160 initial_diurnal_difficulty{INITIAL_DIURNAL_DIFFICULTY};

    CreditSystem(MemoryDataStore &msgdata, MemoryDataStore &creditdata):
            msgdata(msgdata), creditdata(creditdata)
    { }

    std::vector<uint160> MostWorkBatches();

    void StoreMinedCredit(MinedCredit mined_credit);

    void StoreTransaction(SignedTransaction transaction);

    void StoreHash(uint160 hash);

    void StoreMinedCreditMessage(MinedCreditMessage msg);

    CreditBatch ReconstructBatch(MinedCreditMessage &msg);

    uint160 FindFork(uint160 credit_hash1, uint160 credit_hash2);

    uint160 PreviousMinedCreditMessageHash(uint160 credit_hash);

    std::set<uint64_t> GetPositionsOfCreditsSpentBetween(uint160 from_credit_hash, uint160 to_credit_hash);

    std::set<uint64_t> GetPositionsOfCreditsSpentInBatch(uint160 credit_hash);

    bool GetSpentAndUnspentWhenSwitchingAcrossFork(uint160 from_credit_hash, uint160 to_credit_hash,
                                                   std::set<uint64_t> &spent, std::set<uint64_t> &unspent);

    BitChain GetSpentChainOnOtherProngOfFork(BitChain &spent_chain, uint160 from_credit_hash, uint160 to_credit_hash);

    BitChain GetSpentChain(uint160 credit_hash);

    void AddToMainChain(MinedCreditMessage &msg);

    uint160 TotalWork(MinedCreditMessage &msg);

    void RemoveFromMainChain(MinedCreditMessage &msg);

    bool IsInMainChain(uint160 credit_hash);

    bool IsCalend(uint160 credit_hash);

    uint160 InitialDifficulty();

    void SetBatchRootAndSizeAndMessageListHashAndSpentChainHash(MinedCreditMessage &msg);

    void SetExpectedNumberOfMegabytesInMinedCreditProofsOfWork(uint64_t number_of_megabytes);

    uint64_t expected_memory_factor_for_mined_credit_proofs_of_work{MEMORY_FACTOR};

    bool QuickCheckProofOfWorkInMinedCreditMessage(MinedCreditMessage &msg);

    bool QuickCheckProofOfWorkInCalend(Calend calend);

    bool CheckHashesSeedsAndThresholdsInMinedCreditMessageProofOfWork(MinedCreditMessage &msg);

    bool ProofHasCorrectNumberOfSeedsAndLinks(TwistWorkProof proof);

    void SetChildBatch(uint160 parent_hash, uint160 child_hash);

    void RemoveFromMainChain(uint160 credit_hash);

    void RecordTotalWork(uint160 credit_hash, uint160 total_work);

    void RemoveBatchAndChildrenFromMainChainAndDeleteRecordOfTotalWork(uint160 credit_hash);

    void RemoveFromMainChainAndDeleteRecordOfTotalWork(MinedCreditMessage &msg);

    void RemoveFromMainChainAndDeleteRecordOfTotalWork(uint160 credit_hash);

    void SwitchMainChainToOtherBranchOfFork(uint160 current_tip, uint160 new_tip);

    void DeleteRecordOfTotalWork(MinedCreditMessage &msg);

    std::string MessageType(uint160 hash);

    void GetMessagesOnOldAndNewBranchesOfFork(uint160 old_tip, uint160 new_tip, std::vector<uint160>& messages_on_old_branch,
                                              std::vector<uint160>& messages_on_new_tip);

    std::vector<uint160> GetMessagesOnBranch(uint160 branch_start, uint160 branch_end);

    std::vector<uint160> GetMessagesInMinedCreditMessage(uint160 msg_hash);

    BitChain ConstructSpentChainFromPreviousSpentChainAndContentsOfMinedCreditMessage(MinedCreditMessage &msg);

    void AddIncompleteProofOfWork(MinedCreditMessage &msg);

    void RecordCalendarReportedWork(CalendarMessage calendar_message, uint160 reported_work);

    uint160 MaximumReportedCalendarWork();

    CalendarMessage CalendarMessageWithMaximumReportedWork();

    CalendarMessage CalendarMessageWithMaximumScrutinizedWork();

    void RecordCalendarScrutinizedWork(CalendarMessage calendar_message, uint160 scrutinized_work);

    void StoreHash(uint160 hash, MemoryDataStore &hashdata);

    void
    SetMiningParameters(uint64_t number_of_megabytes_, uint160 initial_difficulty_, uint160 initial_diurnal_difficulty_);

    bool MinedCreditWasRecordedToHaveTotalWork(uint160 credit_hash, uint160 total_work);

    void AddMinedCreditMessageAndPredecessorsToMainChain(uint160 credit_hash);

    void MarkCalendAndSucceedingCalendsAsInvalid(uint160 message, uint160 failure_message_hash);

    bool CalendHasReportedFailure(uint160 calend_hash);

    bool ReportedFailedCalendHasBeenReceived(CalendarFailureMessage message);

    void RecordReportedFailureOfCalend(CalendarFailureMessage message);

    bool CalendarContainsAKnownBadCalend(Calendar &calendar);

    void MarkCalendAndSucceedingCalendsAsInvalid(CalendarFailureMessage message);

    void StoreCalendsFromCalendar(Calendar &calendar_);

    void RemoveReportedTotalWorkOfMinedCreditsSucceedingInvalidCalend(CalendarFailureMessage message);

    void AcceptMinedCreditMessageAsValidByRecordingTotalWorkAndParent(MinedCreditMessage msg);

    uint160 PrecedingCalendHash(uint160 msg_hash);

    uint160 GetNextDifficulty(MinedCreditMessage msg);

    uint160 GetNextDiurnalDifficulty(MinedCreditMessage msg);

    EncodedNetworkState SucceedingNetworkState(MinedCreditMessage msg);

    uint160 GetNextPreviousDiurnRoot(MinedCreditMessage &msg);

    bool DataIsPresentFromMinedCreditToPrecedingCalendOrStart(MinedCreditMessage msg);

    uint160 GetNextDiurnalBlockRoot(MinedCreditMessage msg);
};


#endif //FLEX_CREDITSYSTEM_H
