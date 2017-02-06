#ifndef TELEPORT_RELAYADMISSIONHANDLER_H
#define TELEPORT_RELAYADMISSIONHANDLER_H


#include <test/teleport_tests/node/calendar/Calendar.h>
#include <src/teleportnode/schedule.h>
#include "KeyDistributionComplaint.h"


class RelayMessageHandler;

class RelayAdmissionHandler
{
public:
    Data data;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    RelayState *relay_state{NULL};
    RelayMessageHandler *relay_message_handler;
    Scheduler scheduler;
    uint8_t mode{0};

    RelayAdmissionHandler(Data data, CreditSystem *credit_system, Calendar *calendar, RelayMessageHandler *handler);

    void AddScheduledTasks()
    {
        scheduler.AddTask(ScheduledTask("key_distribution",
                                        &RelayAdmissionHandler::HandleKeyDistributionMessageAfterDuration, this));
    }

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    void HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionMessageAfterDuration(uint160 key_distribution_message_hash);

    void HandleKeyDistributionComplaint(KeyDistributionComplaint complaint);

    bool MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message);

    void AcceptRelayJoinMessage(RelayJoinMessage join_message);

    bool ValidateRelayJoinMessage(RelayJoinMessage relay_join_message);

    bool MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(uint160 mined_credit_message_hash);

    bool ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void StoreKeyDistributionSecretsAndSendComplaints(KeyDistributionMessage key_distribution_message);

    void SendKeyDistributionComplaint(uint160 key_distribution_message_hash, uint64_t set_of_secrets,
                                      uint32_t position_of_bad_secret);

    void RecoverSecretsAndSendKeyDistributionComplaints(Relay *relay, uint64_t set_of_secrets,
                                                        std::vector<uint64_t> recipients,
                                                        uint160 key_distribution_message_hash,
                                                        std::vector<uint256> encrypted_secrets);

    bool RecoverAndStoreSecret(Relay *recipient, uint256 &encrypted_secret, Point &point_corresponding_to_secret);

    void RecordSentComplaint(uint160 complaint_hash, uint160 bad_message_hash);

    bool ValidateKeyDistributionComplaint(KeyDistributionComplaint complaint);

};


#endif //TELEPORT_RELAYADMISSIONHANDLER_H
