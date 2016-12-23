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
#include "KeyDistributionRefutation.h"
#include "DurationWithoutResponseFromRelay.h"
#include "GoodbyeComplaint.h"
#include "GoodbyeRefutation.h"
#include "SecretRecoveryComplaint.h"
#include "SecretRecoveryRefutation.h"


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

    std::vector<Point> GetKeyQuarterHoldersAsListOf16Recipients(uint64_t relay_number);

    std::vector<Point> GetKeySixteenthHoldersAsListOf16Recipients(uint64_t relay_number, uint64_t first_or_second_set);

    void ProcessKeyDistributionMessage(KeyDistributionMessage message);

    void ThrowExceptionIfMinedCreditMessageHashWasAlreadyUsed(RelayJoinMessage relay_join_message);

    void UnprocessRelayJoinMessage(RelayJoinMessage relay_join_message);

    void UnprocessKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void ProcessSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

    void UnprocessSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message);

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

    void ProcessDurationAfterKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data);

    void ProcessKeyDistributionRefutation(KeyDistributionRefutation refutation, Data data);

    void RecordRelayDeath(Relay *dead_relay, Data data, uint32_t reason_for_leaving);

    void ProcessDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration, Data data);

    void ProcessDurationWithoutRelayResponseAfterObituary(Obituary obituary, uint64_t relay_number, Data data);

    void ProcessGoodbyeMessage(GoodbyeMessage goodbye);

    void ProcessDurationAfterGoodbyeMessage(GoodbyeMessage goodbye, Data data);

    bool KeyDistributionMessageHasBeenComplainedAbout(KeyDistributionMessage key_distribution_message, Data data);

    void ProcessGoodbyeComplaint(GoodbyeComplaint complaint, Data data);

    void ProcessDurationAfterGoodbyeComplaint(GoodbyeComplaint goodbye_complaint, Data data);

    void ProcessGoodbyeRefutation(GoodbyeRefutation refutation, Data data);

    void ProcessDurationAfterSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, Data data);

    void ProcessSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void ProcessDurationAfterSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void TransferTasks(uint64_t dead_relay_number, uint64_t successor_number);

    void ProcessSecretRecoveryRefutation(SecretRecoveryRefutation refutation, Data data);

    void UnprocessDurationWithoutResponse(DurationWithoutResponse duration, Data data);

    void UnprocessDurationAfterSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void UnprocessObituary(Obituary obituary);

    void UnrecordRelayDeath(Relay *dead_relay, Data data, uint32_t reason);

    void UnprocessSecretRecoveryRefutation(SecretRecoveryRefutation refutation, Data data);

    void UnprocessDurationAfterSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, Data data);

    void UnprocessSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data);

    void UnprocessDurationWithoutRelayResponseAfterObituary(Obituary obituary, uint64_t relay_number, Data data);

    void UnprocessDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration, Data data);

    void UnprocessRelayExit(RelayExit relay_exit, Data data);

    void ReaddRelay(Obituary obituary, uint64_t successor_relay_number);

    void TransferTasksBackFromSuccessorToRelay(Obituary obituary);

    void UnprocessGoodbyeRefutation(GoodbyeRefutation refutation, Data data);

    void UnprocessDurationAfterGoodbyeComplaint(GoodbyeComplaint goodbye_complaint, Data data);

    void UnprocessGoodbyeComplaint(GoodbyeComplaint complaint, Data data);

    void UnprocessDurationAfterGoodbyeMessage(GoodbyeMessage goodbye, Data data);

    void UnprocessGoodbyeMessage(GoodbyeMessage goodbye);

    void UnprocessKeyDistributionRefutation(KeyDistributionRefutation refutation, Data data);

    void UnprocessDurationAfterKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data);

    void UnprocessDurationAfterKeyDistributionMessage(KeyDistributionMessage key_distribution_message, Data data);

    void UnprocessKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data);

    bool MinedCreditMessageHashIsAlreadyBeingUsed(uint160 mined_credit_message_hash);
};

class RelayStateException : public std::runtime_error
{
public:
    explicit RelayStateException(const std::string& msg)
            : std::runtime_error(msg)
    { }
};



#endif //TELEPORT_RELAYSTATE_H