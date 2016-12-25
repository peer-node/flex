#ifndef TELEPORT_RELAYMESSAGEHANDLER_H
#define TELEPORT_RELAYMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/teleportnode/schedule.h>
#include "RelayState.h"
#include "CreditSystem.h"
#include "MessageHandlerWithOrphanage.h"
#include "RelayJoinMessage.h"
#include "KeyDistributionMessage.h"

#define RESPONSE_WAIT_TIME 8000000 // microseconds

class RelayMessageHandler : public MessageHandlerWithOrphanage
{
public:
    Data data;
    CreditSystem *credit_system;
    Calendar *calendar;
    RelayState relay_state;
    Scheduler scheduler;

    RelayMessageHandler(Data data):
            MessageHandlerWithOrphanage(data.msgdata), data(data.msgdata, data.creditdata, data.keydata, &relay_state)
    {
        channel = "relay";
    }

    void AddScheduledTasks()
    {
        scheduler.AddTask(ScheduledTask("key_distribution",
                                        &RelayMessageHandler::HandleKeyDistributionMessageAfterDuration, this));
    }

    void SetCreditSystem(CreditSystem *credit_system);

    void SetCalendar(Calendar *calendar);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(RelayJoinMessage);
        HANDLESTREAM(KeyDistributionMessage);
        log_ << "finished handling\n";
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(RelayJoinMessage);
        HANDLEHASH(KeyDistributionMessage);
    }

    HANDLECLASS(RelayJoinMessage);
    HANDLECLASS(KeyDistributionMessage);

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    void HandleKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    bool MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message);

    void AcceptRelayJoinMessage(RelayJoinMessage join_message);

    bool ValidateRelayJoinMessage(RelayJoinMessage relay_join_message);

    bool MinedCreditMessageIsNewerOrMoreThanThreeBatchesOlderThanLatest(uint160 mined_credit_message_hash);

    bool ValidateKeyDistributionMessage(KeyDistributionMessage key_distribution_message);

    void HandleKeyDistributionMessageAfterDuration(uint160 key_distribution_message_hash);

    void SendKeyDistributionComplaintsIfAppropriate(KeyDistributionMessage key_distribution_message);

    void SendKeyDistributionComplaints(Relay *relay, uint64_t set_of_secrets, std::vector<Point> recipients,
                                       uint160 key_distribution_message_hash, std::vector<CBigNum> encrypted_secrets);

    bool
    RecoverAndStoreSecret(Point &recipient_public_key, CBigNum &encrypted_secret, Point &point_corresponding_to_secret);

    void SendKeyDistributionComplaint(uint160 key_distribution_message_hash, uint64_t set_of_secrets,
                                      uint32_t position_of_bad_secret);
};


#endif //TELEPORT_RELAYMESSAGEHANDLER_H
