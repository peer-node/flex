#ifndef TELEPORT_RELAYSTATE_H
#define TELEPORT_RELAYSTATE_H


#include <vector>
#include "Relay.h"
#include "RelayJoinMessage.h"
#include "SecretRecoveryMessage.h"
#include "Obituary.h"
#include "RelayExit.h"
#include "KeyDistributionComplaint.h"
#include "DurationWithoutResponse.h"
#include "DurationWithoutResponseFromRelay.h"
#include "GoodbyeComplaint.h"
#include "SecretRecoveryComplaint.h"
#include "RecoveryFailureAuditMessage.h"


class RelayState
{
public:
    uint160 latest_mined_credit_message_hash{0};
    std::vector<Relay> relays;
    std::map<uint64_t, Relay*> relays_by_number;
    std::map<uint160, Relay*> relays_by_join_hash;
    bool maps_are_up_to_date{false};

    RelayState& operator=(const RelayState &other)
    {
        relays = other.relays;
        maps_are_up_to_date = false;
        return *this;
    }

    bool operator==(const RelayState &other) const
    {
        return relays == other.relays;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(relays);
    )

    JSON(relays);

    HASH160();

    void EnsureMapsAreUpToDate();

    void ProcessRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message);

    Relay* GetRelayByNumber(uint64_t number);

    Relay *GetRelayByJoinHash(uint160 join_message_hash);

    void AssignRemainingQuarterKeyHoldersToRelay(Relay &relay, uint160 encoding_message_hash);

    bool AssignKeyPartHoldersToRelay(Relay &relay, uint160 encoding_message_hash);

    uint64_t NumberOfRelaysThatJoinedLaterThan(Relay &relay);

    uint64_t NumberOfRelaysThatJoinedBefore(Relay &relay);

    bool ThereAreEnoughRelaysToAssignKeyPartHolders(Relay &relay);

    uint64_t KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed);

    bool KeyPartHolderIsAlreadyBeingUsed(Relay &key_distributor, Relay &key_part_holder);

    uint64_t SelectKeyHolderWhichIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed);

    void AssignRemainingKeySixteenthHoldersToRelay(Relay &relay, uint160 encoding_message_hash);

    void AssignKeySixteenthAndQuarterHoldersWhoJoinedLater(Relay &relay, uint160 encoding_message_hash);

    void RemoveKeyPartHolders(Relay &relay);

    std::vector<uint64_t> GetKeyQuarterHoldersAsListOf16RelayNumbers(uint64_t relay_number);

    std::vector<uint64_t> GetKeySixteenthHoldersAsListOf16RelayNumbers(uint64_t relay_number,
                                                                       uint64_t first_or_second_set);

    void ProcessKeyDistributionMessage(KeyDistributionMessage message);

    void ThrowExceptionIfMinedCreditMessageHashWasAlreadyUsed(RelayJoinMessage relay_join_message);

    void ProcessSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    uint64_t AssignSuccessorToRelay(Relay *relay);

    bool SuccessorToRelayIsSuitable(Relay *chosen_successor, Relay *relay);

    std::vector<uint64_t> RelayNumbersOfRelaysWhoseKeyQuartersAreHeldByGivenRelay(Relay *relay);

    Obituary GenerateObituary(Relay *relay, uint32_t reason_for_leaving);

    bool RelayIsOldEnoughToLeaveInGoodStanding(Relay *relay);

    void ProcessObituary(Obituary obituary);

    RelayExit GenerateRelayExit(Relay *relay);

    void ProcessRelayExit(RelayExit relay_exit, Data data);

    void RemoveRelay(uint64_t exiting_relay_number, uint64_t successor_relay_number);

    void ProcessKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data);

    void ProcessDurationWithoutResponse(DurationWithoutResponse duration, Data data);

    void ProcessDurationAfterKeyDistributionMessage(KeyDistributionMessage key_distribution_message, Data data);

    void RecordRelayDeath(Relay *dead_relay, Data data, uint32_t reason_for_leaving);

    void ProcessDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration, Data data);

    void ProcessDurationWithoutRelayResponseAfterObituary(Obituary obituary, uint64_t relay_number, Data data);

    void ProcessGoodbyeMessage(GoodbyeMessage goodbye);

    void ProcessDurationAfterGoodbyeMessage(GoodbyeMessage goodbye, Data data);

    void ProcessGoodbyeComplaint(GoodbyeComplaint complaint, Data data);

    void ProcessDurationAfterSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, Data data);

    void ProcessSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void ProcessDurationAfterSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void TransferTasks(uint64_t dead_relay_number, uint64_t successor_number);

    void ReaddRelay(Obituary obituary, uint64_t successor_relay_number);

    void TransferTasksBackFromSuccessorToRelay(Obituary obituary);

    bool MinedCreditMessageHashIsAlreadyBeingUsed(uint160 mined_credit_message_hash);

    void ProcessRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message, Data data);

    void ProcessDurationWithoutRelayResponseAfterSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message,
                                                                              uint64_t relay_number, Data data);
};

class RelayStateException : public std::runtime_error
{
public:
    explicit RelayStateException(const std::string& msg)
            : std::runtime_error(msg)
    { }
};



#endif //TELEPORT_RELAYSTATE_H