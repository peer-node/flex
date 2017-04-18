#ifndef TELEPORT_RELAYSTATE_H
#define TELEPORT_RELAYSTATE_H


#include "Relay.h"
#include "RelayJoinMessage.h"
#include "SecretRecoveryMessage.h"
#include "KeyDistributionComplaint.h"
#include "DurationWithoutResponse.h"
#include "DurationWithoutResponseFromRelay.h"
#include "SecretRecoveryComplaint.h"
#include "RecoveryFailureAuditMessage.h"
#include "SuccessionCompletedAuditMessage.h"
#include <test/teleport_tests/node/relays/KeyDistributionAuditorSelection.h>


class RelayState
{
public:
    uint160 latest_mined_credit_message_hash{0};
    std::vector<Relay> relays;
    std::map<uint64_t, Relay*> relays_by_number;
    std::map<uint160, Relay*> relays_by_join_hash;
    bool maps_are_up_to_date{false};
    bool remove_dead_relays{true};

    RelayState& operator=(const RelayState &other)
    {
        relays = other.relays;
        maps_are_up_to_date = false;
        return *this;
    }

    bool operator==(const RelayState &other) const
    {
        return relays == other.relays and latest_mined_credit_message_hash == other.latest_mined_credit_message_hash;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(latest_mined_credit_message_hash);
        READWRITE(relays);
    )

    JSON(latest_mined_credit_message_hash, relays);

    HASH160();

    void EnsureMapsAreUpToDate();

    void ProcessRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay* GetRelayByNumber(uint64_t number);

    Relay *GetRelayByJoinHash(uint160 join_message_hash);

    uint64_t NumberOfEligibleRelaysThatJoinedLaterThan(Relay &relay);

    uint64_t NumberOfEligibleRelaysThatJoinedBefore(Relay &relay);

    bool ThereAreEnoughRelaysToAssignKeyQuarterHolders(Relay &relay);

    uint64_t KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed);

    uint64_t SelectRandomKeyQuarterHolderWhichIsntAlreadyBeingUsed(Relay &relay, uint160 relay_chooser);

    std::vector<uint64_t> GetKeyQuarterHoldersAsListOf24RelayNumbers(uint64_t relay_number);

    void ProcessKeyDistributionMessage(KeyDistributionMessage message);

    void ThrowExceptionIfMinedCreditMessageHashWasAlreadyUsed(RelayJoinMessage relay_join_message);

    void ProcessSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    uint64_t AssignSuccessorToRelay(Relay *relay);

    bool SuccessorToRelayIsSuitable(Relay *chosen_successor, Relay *relay);

    std::vector<uint64_t> RelayNumbersOfRelaysWhoseKeyQuartersAreHeldByGivenRelay(Relay *relay);

    void ProcessKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data);

    void ProcessDurationWithoutResponse(DurationWithoutResponse duration, Data data);

    void ProcessDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration, Data data);

    void ProcessSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void TransferTasksToSuccessor(uint64_t dead_relay_number, uint64_t successor_number);

    bool MinedCreditMessageHashIsAlreadyBeingUsed(uint160 mined_credit_message_hash);

    void ProcessRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message, Data data);

    void ProcessDurationWithoutRelayResponseAfterSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message,
                                                                              uint64_t relay_number, Data data);

    void ProcessSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message, Data data);

    bool ContainsRelayWithNumber(uint64_t relay_number);

    bool SuccessorWasLyingAboutSumOfRecoveredSharedSecretQuarters(Relay *dead_relay,
                                                                  uint160 failure_message_hash, Data data);

    void DetermineWhetherSuccessorOrKeySharerIsAtFaultInSecretRecoveryFailure(Relay *dead_relay,
                                                                              uint160 failure_message_hash, Data data);

    void ProcessSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message, Data data);

    KeyDistributionAuditorSelection GenerateKeyDistributionAuditorSelection(Relay *key_sharer);

    void ProcessKeyDistributionAuditMessage(KeyDistributionAuditMessage audit_message);

    void RecordSentComplaint(Relay *complainer);

    void ProcessDurationWithoutResponseAfterKeyDistributionAuditMessage(KeyDistributionAuditMessage audit_message,
                                                                        Data data);

    void AssignSecretRecoveryTasksToKeyQuarterHolders(Relay *dead_relay, Data data);

    void ReplaceDeadRelayWithSuccessorInQuarterHolderRoles(uint64_t dead_relay_number, uint64_t successor_relay_number);

    void ProcessGoodbyeMessage(GoodbyeMessage goodbye, Data data);

    void AssignRecoveryFailureAuditTasksToQuarterHolders(SecretRecoveryFailureMessage failure_message, Data data);

    void RecordSecretRecoveryMessageTaskCompleted(SecretRecoveryMessage &secret_recovery_message);

    void RecordRecoveryFailureAuditMessageTaskCompleted(RecoveryFailureAuditMessage audit_message, Data data);

    void ExamineAuditMessageToDetermineWrongdoer(RecoveryFailureAuditMessage audit_message, Relay *dead_relay, Data data);

    void AssignCompleteSuccessionTaskIfSuccessionAttemptHasFourMessages(Relay *dead_relay, Relay *successor);

    void ProcessDurationWithoutResponseAfterFourSecretRecoveryMessages(FourSecretRecoveryMessages secret_recovery_messages,
                                                                       Data data);

    void ProcessSuccessionCompletedAuditMessage(SuccessionCompletedAuditMessage &audit_message, Data data);

    uint64_t GetEligibleSuccessorForRelay(Relay *relay);

    void AssignNewKeyQuarterHoldersAndSuccessorsAfterNewRelayHasJoined(Relay *new_relay);

    bool RelayIsASuitableQuarterHolder(Relay *key_sharer, int64_t candidate_relay_number);

    void AssignRemainingKeyQuarterHoldersToRelay(Relay &relay);

    void AssignKeyQuarterHolderWhoJoinedLater(Relay &relay);

    bool AssignKeyQuarterHoldersToRelay(Relay &relay);

    void ProcessDurationWithoutRelayResponseAfterDeath(uint64_t relay_number, Data data);

    void RecordRelayDeath(Relay *dead_relay, Data data, uint8_t reason);

    void ProcessMinedCreditMessage(MinedCreditMessage &msg, Data data);

    void MarkRelayJoinMessageAsEncoded(uint160 message_hash, Data data);

    void AssignKeyDistributionTasks();

    bool RelayShouldSendAKeyDistributionMessage(Relay &relay);

    bool RelayHasFourLivingQuarterHoldersWhoHaveReceivedKeyQuarters(Relay &relay);

    bool RelayHasSharedAKeyQuarterWithAQuarterHolderWhoIsNowDeadAndHasNoSuccessor(Relay &relay);

    bool RelayHasFourLivingQuarterHoldersWithEncodedJoinMessagesWhoHaveNotReceivedKeyQuarters(Relay &relay);

    bool RelayHasFourLivingKeyQuarterHolders(Relay &relay);

    bool RelayHasFourLivingKeyQuarterHoldersWithEncodedJoinMessages(Relay &relay);

    bool NewQuarterHoldersAreAvailableToReplaceDeadOnesWithoutSuccessors(Relay &relay);

    void MarkKeyDistributionMessageAsEncoded(uint160 key_distribution_message_hash, Data data);

    void RemoveKeyDistributionTask(Relay *relay);

    void RecordKeyDistributionMessage(Relay *relay, uint160 key_distribution_message_hash);

    void AssignKeyDistributionAuditTasks();

    bool RelayShouldSendAKeyDistributionAuditMessage(Relay &relay);

    bool RelayHasAnEncodedKeyDistributionMessageWithNoCorrespondingAuditMessage(Relay &relay);

    void RemoveKeyDistributionAuditTask(KeyDistributionAuditMessage &audit_message);

    void AssignKeyDistributionTaskToRelay(Relay &relay);

    void AssignKeyDistributionAuditTaskToRelay(Relay &relay);
};

class RelayStateException : public std::runtime_error
{
public:
    explicit RelayStateException(const std::string& msg)
            : std::runtime_error(msg)
    { }
};



#endif //TELEPORT_RELAYSTATE_H