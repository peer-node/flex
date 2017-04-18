#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relays/RelayState.h"
#include "test/teleport_tests/node/relays/RelayMemoryCache.h"
#include "test/teleport_tests/node/relays/KeyQuarterLocation.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayState : public Test
{
public:
    RelayState relay_state;
    RelayJoinMessage relay_join_message;
    MemoryDataStore keydata, creditdata, msgdata;
    Data *data;

    virtual void SetUp()
    {
        data = new Data(msgdata, creditdata, keydata, &relay_state);
        data->CreateCache();
    }
    virtual void TearDown()
    {
        delete data;
    }
};

TEST_F(ARelayState, InitiallyHasNoRelays)
{
    vector<Relay> relays = relay_state.relays;
    ASSERT_THAT(relays.size(), Eq(0));
}

TEST_F(ARelayState, AddsARelayWhenProcessingARelayJoinMessage)
{
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    vector<Relay> relays = relay_state.relays;
    ASSERT_THAT(relays.size(), Eq(1));
}

TEST_F(ARelayState, AssignsARelayNumberToAJoiningRelay)
{
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    ASSERT_THAT(relay_state.relays[0].number, Eq(1));
}

TEST_F(ARelayState, PopulatesTheRelaysJoinMessageHash)
{
    RelayJoinMessage relay_join_message;
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    uint160 join_message_hash = relay_state.relays[0].hashes.join_message_hash;
    ASSERT_THAT(join_message_hash, Eq(relay_join_message.GetHash160()));
}


class ARelayStateWith53Relays : public ARelayState
{
public:
    virtual void SetUp()
    {
        ARelayState::SetUp();
        static RelayState reference_test_relay_state;
        static MemoryDataStore reference_test_keydata, reference_msgdata;

        if (reference_test_relay_state.relays.size() == 0)
        {
            for (uint32_t i = 1; i <= 53; i++)
            {
                RelayJoinMessage join_message = Relay().GenerateJoinMessage(keydata, i);
                relay_state.ProcessRelayJoinMessage(join_message);
                data->StoreMessage(join_message);
            }
            reference_test_relay_state = relay_state;
            reference_test_keydata = keydata;
            reference_msgdata = msgdata;
        }
        else
        {
            relay_state = reference_test_relay_state;
            keydata = reference_test_keydata;
            msgdata = reference_msgdata;
        }
    }

    bool RelayHasAQuarterHolderThatJoinedLaterThanIt(Relay *relay)
    {
        for (auto quarter_holder_number : relay->key_quarter_holders)
            if (quarter_holder_number > relay->number)
                return true;
        return false;
    }

    virtual void TearDown()
    {
        ARelayState::TearDown();
    }
};

TEST_F(ARelayStateWith53Relays, AssignsKeyQuarterHoldersToEachRelayWithARelayWhoJoinedLaterThanIt)
{
    for (auto &relay : relay_state.relays)
    {
        bool holders_assigned = relay_state.AssignKeyQuarterHoldersToRelay(relay);
        ASSERT_THAT(holders_assigned, Eq(relay.number <= 52)) << "failed at " << relay.number;
    }
}

TEST_F(ARelayStateWith53Relays, AssignsKeyQuarterHoldersAutomatically)
{
    for (auto &relay : relay_state.relays)
    {
        bool holders_assigned = relay.HasFourKeyQuarterHolders();
        ASSERT_THAT(holders_assigned, Eq(relay.number <= 52)) << "failed at " << relay.number;
    }
}

TEST_F(ARelayStateWith53Relays, AssignsAtLeastOneKeyQuarterHolderWhichJoinedLaterThanTheKeySharer)
{
    for (auto &relay : relay_state.relays)
    {
        bool holders_assigned = relay_state.AssignKeyQuarterHoldersToRelay(relay);
        if (holders_assigned)
            ASSERT_TRUE(RelayHasAQuarterHolderThatJoinedLaterThanIt(&relay));
    }
}

TEST_F(ARelayStateWith53Relays, AssignsSuccessorsToEachRelayWithTwoRelaysWhichJoinedLater)
{
    for (auto &relay : relay_state.relays)
    {
        relay_state.AssignSuccessorToRelay(&relay);
        if (relay.current_successor_number == 0)
            ASSERT_THAT(relay.number, Gt(51));
    }
}

TEST_F(ARelayStateWith53Relays, AssignsSuccessorsAutomatically)
{
    for (auto &relay : relay_state.relays)
    {
        if (relay.current_successor_number == 0)
            ASSERT_THAT(relay.number, Gt(51));
    }
}


class ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage : public ARelayStateWith53Relays
{
public:
    MinedCreditMessage msg;

    virtual void SetUp()
    {
        ARelayStateWith53Relays::SetUp();

        msg = MinedCreditMessageContainingAJoinMessage();
    }

    MinedCreditMessage MinedCreditMessageContainingAJoinMessage()
    {
        MinedCreditMessage msg;
        for (uint32_t i = 0; i < 20; i++)
            msg.hash_list.full_hashes.push_back(relay_state.relays[i].hashes.join_message_hash);
        msg.hash_list.GenerateShortHashes();
        return msg;
    }

    virtual void TearDown()
    {
        ARelayStateWith53Relays::TearDown();
    }
};

TEST_F(ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage,
       SaysThatARelayShouldSendAKeyDistributionMessageIfItsQuarterHoldersHaveJoinMessagesWhichWereEncoded)
{
    relay_state.ProcessMinedCreditMessage(msg, *data);
    for (auto &relay : relay_state.relays)
    {
        if (not relay.HasFourKeyQuarterHolders())
            continue;
        bool quarter_holders_have_encoded_messages = true;
        for (auto quarter_holder_number : relay.key_quarter_holders)
        {
            auto quarter_holder = relay_state.GetRelayByNumber(quarter_holder_number);
            if (quarter_holder == NULL) continue;
            if (not quarter_holder->join_message_was_encoded)
                quarter_holders_have_encoded_messages = false;
        }
        ASSERT_THAT(relay_state.RelayShouldSendAKeyDistributionMessage(relay),
                    Eq(quarter_holders_have_encoded_messages)) << "failed at " << relay.number;
    }
}

TEST_F(ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage,
       MarksTheJoinMessageAsEncodedWhenProcessingTheMinedCreditMessage)
{
    bool join_message_encoded = relay_state.relays[0].join_message_was_encoded;
    ASSERT_FALSE(join_message_encoded);
    relay_state.ProcessMinedCreditMessage(msg, *data);
    join_message_encoded = relay_state.relays[0].join_message_was_encoded;
    ASSERT_TRUE(join_message_encoded);
}

TEST_F(ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage,
       UpdatesTheLatestMinedCreditMessageHashWhenProcessingTheMinedCreditMessage)
{
    ASSERT_THAT(relay_state.latest_mined_credit_message_hash, Ne(msg.GetHash160()));
    relay_state.ProcessMinedCreditMessage(msg, *data);
    ASSERT_THAT(relay_state.latest_mined_credit_message_hash, Eq(msg.GetHash160()));
}

TEST_F(ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage,
       AssignsTheKeyDistributionTaskToEachRelayWhichShouldSendOne)
{
    relay_state.ProcessMinedCreditMessage(msg, *data);
    for (auto &relay : relay_state.relays)
    {
        if (relay_state.RelayShouldSendAKeyDistributionMessage(relay))
        {
            Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay.hashes.join_message_hash, ALL_POSITIONS);
            ASSERT_TRUE(VectorContainsEntry(relay.tasks, task));
        }
    }
}


class ARelayStateWith53RelaysWhichHasProcessedAKeyDistributionMessage : public ARelayStateWith53Relays
{
public:
    Relay *relay;
    KeyDistributionMessage key_distribution_message;

    virtual void SetUp()
    {
        ARelayStateWith53Relays::SetUp();
        relay = relay_state.GetRelayByNumber(22);
        key_distribution_message = relay->GenerateKeyDistributionMessage(*data, 1, relay_state);
        relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    }

    virtual void TearDown()
    {
        ARelayStateWith53Relays::TearDown();
    }
};

TEST_F(ARelayStateWith53RelaysWhichHasProcessedAKeyDistributionMessage, AssignsDistinctAuditors)
{
    auto auditor_selection = relay_state.GenerateKeyDistributionAuditorSelection(relay);
    std::set<uint64_t> auditors;
    for (auto auditor_number : auditor_selection.first_set_of_auditors)
    {
        ASSERT_FALSE(auditors.count(auditor_number)) << auditor_number << " occurred twice";
        auditors.insert(auditor_number);
    }
    for (auto auditor_number : auditor_selection.second_set_of_auditors)
    {
        ASSERT_FALSE(auditors.count(auditor_number)) << auditor_number << " occurred twice";
        auditors.insert(auditor_number);
    }
}

TEST_F(ARelayStateWith53RelaysWhichHasProcessedAKeyDistributionMessage,
       AssignsAuditorsNoneOfWhichAreKeyQuarterHolders)
{
    auto auditor_selection = relay_state.GenerateKeyDistributionAuditorSelection(relay);
    for (auto auditor_number : auditor_selection.first_set_of_auditors)
    {
        ASSERT_THAT(auditor_number, Ne(0));
        for (auto quarter_holder_number : relay->key_quarter_holders)
            ASSERT_THAT(auditor_number, Ne(quarter_holder_number));
    }
}

TEST_F(ARelayStateWith53RelaysWhichHasProcessedAKeyDistributionMessage,
       AssignsAuditorsTwoOfWhichJoinedLaterThanTheKeySharer)
{
    auto auditor_selection = relay_state.GenerateKeyDistributionAuditorSelection(relay);
    auto first_auditor_in_first_set = relay_state.GetRelayByNumber(auditor_selection.first_set_of_auditors[0]);
    auto first_auditor_in_second_set = relay_state.GetRelayByNumber(auditor_selection.second_set_of_auditors[0]);

    ASSERT_THAT(first_auditor_in_first_set->number, Gt(relay->number));
    ASSERT_THAT(first_auditor_in_second_set->number, Gt(relay->number));
}


class ARelayStateWith53RelaysAndAKeyDistributionMessage : public ARelayStateWith53Relays
{
public:
    Relay *relay;
    uint64_t number_of_chosen_relay;
    KeyDistributionMessage key_distribution_message;

    virtual void SetUp()
    {
        ARelayStateWith53Relays::SetUp();
        SetUpRelayState();
        number_of_chosen_relay = TheNumberOfARelayWhichIsBeingUsedAsAKeyQuarterHolder();
        relay = relay_state.GetRelayByNumber(number_of_chosen_relay);
        key_distribution_message = relay->GenerateKeyDistributionMessage(*data, 5, relay_state);
        data->StoreMessage(key_distribution_message);
    }

    virtual void SetUpRelayState()
    {
        static RelayState reference_test_relay_state;

        if (reference_test_relay_state.relays.size() == 0)
        {
            reference_test_relay_state = relay_state;
        }
        else
            relay_state = reference_test_relay_state;
    }

    uint64_t TheNumberOfARelayWhichIsBeingUsedAsAKeyQuarterHolder()
    {
        auto key_distribution_message = relay_state.relays[0].GenerateKeyDistributionMessage(*data, 6, relay_state);
        data->StoreMessage(key_distribution_message);
        relay_state.ProcessKeyDistributionMessage(key_distribution_message);
        return relay_state.relays[0].key_quarter_holders[1];
    }

    virtual void TearDown()
    {
        ARelayStateWith53Relays::TearDown();
    }
};

TEST_F(ARelayStateWith53RelaysAndAKeyDistributionMessage,
       RecordsTheKeyQuarterLocationsWhenProcessingAKeyDistributionMessage)
{
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);

    for (uint32_t position = 0; position < 4; position++)
    {
        auto location = relay->key_quarter_locations[position];
        ASSERT_THAT(location.message_hash, Eq(key_distribution_message.GetHash160()));
        ASSERT_THAT(location.position, Eq(position));
    }
}

TEST_F(ARelayStateWith53RelaysAndAKeyDistributionMessage,
       RemovesTheKeyDistributionTaskWhenProcessingTheKeyDistributionMesssage)
{
    Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay->hashes.join_message_hash, ALL_POSITIONS);
    relay->tasks.push_back(task);
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    ASSERT_THAT(relay->tasks.size(), Eq(0));
}


class ARelayStateWhichHasProcessedAKeyDistributionMessage : public ARelayStateWith53RelaysAndAKeyDistributionMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWith53RelaysAndAKeyDistributionMessage::SetUp();
        relay_state.ProcessKeyDistributionMessage(key_distribution_message);
        data->StoreMessage(key_distribution_message);
    }

    MinedCreditMessage MinedCreditMessageWhichEncodesTheKeyDistributionMessage()
    {
        MinedCreditMessage msg;
        msg.hash_list.full_hashes.push_back(key_distribution_message.GetHash160());
        msg.hash_list.GenerateShortHashes();
        data->StoreMessage(msg);
        return msg;
    }

    virtual void TearDown()
    {
        ARelayStateWith53RelaysAndAKeyDistributionMessage::TearDown();
    }
};


TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       MarksTheKeyDistributionMessageAsEncodedWhenProcessingAMinedCreditMessageWhichEncodesIt)
{
    auto msg = MinedCreditMessageWhichEncodesTheKeyDistributionMessage();
    relay_state.ProcessMinedCreditMessage(msg, *data);
    ASSERT_THAT(relay->key_quarter_locations[0].message_was_encoded, Eq(true));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsTheSendAuditMessageTaskWhenProcessingAMinedCreditMessageWhichEncodesTheKeyDistributionMessage)
{
    auto msg = MinedCreditMessageWhichEncodesTheKeyDistributionMessage();
    relay_state.ProcessMinedCreditMessage(msg, *data);
    Task task(SEND_KEY_DISTRIBUTION_AUDIT_MESSAGE, key_distribution_message.GetHash160(), ALL_POSITIONS);
    ASSERT_TRUE(VectorContainsEntry(relay->tasks, task));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       RemovesTheSendAuditMessageTaskWhenProcessingTheKeyDistributionAuditMessage)
{
    auto msg = MinedCreditMessageWhichEncodesTheKeyDistributionMessage();
    relay_state.ProcessMinedCreditMessage(msg, *data);
    KeyDistributionAuditMessage audit_message;
    audit_message.relay_number = relay->number;
    audit_message.key_distribution_message_hash = key_distribution_message.GetHash160();
    relay_state.ProcessKeyDistributionAuditMessage(audit_message);
    Task task(SEND_KEY_DISTRIBUTION_AUDIT_MESSAGE, key_distribution_message.GetHash160(), ALL_POSITIONS);
    ASSERT_FALSE(VectorContainsEntry(relay->tasks, task));
}

bool RelayHasStatus(Relay *relay, Data data, uint32_t reason)
{
    return relay->status == reason;
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       MarksTheKeySharerAsMisbehavingWhenProcessingAKeyDistributionComplaint)
{
    uint64_t position = 2;
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), position, *data);

    relay_state.ProcessKeyDistributionComplaint(complaint, *data);
    Relay *key_sharer = complaint.GetSecretSender(*data);

    ASSERT_THAT(key_sharer->status, Eq(MISBEHAVED));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       IncrementsTheNumberOfComplaintsSentByTheComplainerWhenProcessingAKeyDistributionComplaint)
{
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), 2, *data);

    Relay *complainer = complaint.GetComplainer(*data);
    ASSERT_THAT(complainer->number_of_complaints_sent, Eq(0));

    relay_state.ProcessKeyDistributionComplaint(complaint, *data);
    ASSERT_THAT(complainer->number_of_complaints_sent, Eq(1));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       RecordsARelaysStatusWhenRecordingItsDeath)
{
    relay_state.RecordRelayDeath(relay, *data, MISBEHAVED);
    ASSERT_THAT(relay->status, Eq(MISBEHAVED));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsTheDeathAsATaskForTheKeyQuarterHolders)
{
    relay_state.RecordRelayDeath(relay, *data, MISBEHAVED);
    for (uint32_t position = 0; position < 4; position++)
    {
        auto key_quarter_holder = relay_state.GetRelayByNumber(relay->key_quarter_holders[position]);
        Task task(SEND_SECRET_RECOVERY_MESSAGE, Death(relay->number), position);
        ASSERT_TRUE(VectorContainsEntry(key_quarter_holder->tasks, task));
    }
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsADistinctSuccessorWhichIsNotAKeyQuarterHolderToARelayWhichIsExiting)
{
    uint64_t successor_relay_number = relay_state.AssignSuccessorToRelay(relay);
    ASSERT_FALSE(ArrayContainsEntry(relay->key_quarter_holders, successor_relay_number));
    ASSERT_THAT(relay->number, Ne(successor_relay_number));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsASuccessorSuchThatNoTwoRelaysAreEachOthersKeyQuarterHolders)
{
    uint64_t successor_relay_number = relay_state.AssignSuccessorToRelay(relay);

    for (Relay &key_sharer : relay_state.relays)
        if (ArrayContainsEntry(key_sharer.key_quarter_holders, relay->number))
        {
            auto position = PositionOfEntryInArray(relay->number, key_sharer.key_quarter_holders);
            key_sharer.key_quarter_holders[position] = successor_relay_number;
        }

    for (Relay &first_relay : relay_state.relays)
        for (Relay &second_relay : relay_state.relays)
        {
            bool first_has_second_as_quarter_holder = ArrayContainsEntry(first_relay.key_quarter_holders,
                                                                         second_relay.number);
            bool second_has_first_as_quarter_holder = ArrayContainsEntry(second_relay.key_quarter_holders,
                                                                         first_relay.number);
            ASSERT_FALSE(first_has_second_as_quarter_holder and second_has_first_as_quarter_holder);
        }
}


class ARelayStateWithAKeyDistributionAuditMessage : public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    KeyDistributionAuditMessage audit_message;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();

        auto msg = MessageContainingKeyDistributionMessageAndRelayState();
        audit_message = relay->GenerateKeyDistributionAuditMessage(msg.GetHash160(), *data);
        data->StoreMessage(audit_message);
    }

    MinedCreditMessage MessageContainingKeyDistributionMessageAndRelayState()
    {
        data->cache->Store(relay_state);
        MinedCreditMessage msg;
        msg.mined_credit.network_state.relay_state_hash = relay_state.GetHash160();
        data->StoreMessage(msg);
        return msg;
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayStateWithAKeyDistributionAuditMessage, RecordsTheAuditMessageHashWhenProcessingIt)
{
    relay_state.ProcessKeyDistributionAuditMessage(audit_message);
    uint160 recorded_audit_message_hash = relay->hashes.key_distribution_audit_message_hash;
    ASSERT_THAT(recorded_audit_message_hash, Eq(audit_message.GetHash160()));
}


class ARelayStateWhichHasProcessedAKeyDistributionAuditMessage : public ARelayStateWithAKeyDistributionAuditMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWithAKeyDistributionAuditMessage::SetUp();
        relay_state.ProcessKeyDistributionAuditMessage(audit_message);
    }

    virtual void TearDown()
    {
        ARelayStateWithAKeyDistributionAuditMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionAuditMessage,
       MarksTheKeyDistributionAsAuditedWhenProcessingADurationWithoutAResponse)
{
    auto duration = GetDurationWithoutResponseAfter(audit_message.GetHash160());
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    ASSERT_THAT(relay->key_distribution_message_audited, Eq(true));
}




class ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage :
        public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    KeyDistributionComplaint complaint;
    Relay *complainer;
    Relay *secret_sender;
    RelayState relay_state_before_complaint;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();
        uint64_t position = 2;
        complaint.Populate(key_distribution_message.GetHash160(), position, *data);
        data->StoreMessage(complaint);

        relay_state_before_complaint = relay_state;
        relay_state.ProcessKeyDistributionComplaint(complaint, *data);

        secret_sender = complaint.GetSecretSender(*data);
        complainer = complaint.GetComplainer(*data);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};


class ARelayStateWithAGoodbyeMessage : public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    GoodbyeMessage goodbye;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();
        goodbye = relay->GenerateGoodbyeMessage(*data);
        data->StoreMessage(goodbye);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayStateWithAGoodbyeMessage, RecordsTheGoodbyeMessageHashOfTheDeadRelay)
{
    relay_state.ProcessGoodbyeMessage(goodbye, *data);
    uint160 goodbye_message_hash = relay->hashes.goodbye_message_hash;
    ASSERT_THAT(goodbye_message_hash, Eq(goodbye.GetHash160()));
}


class ARelayStateWhichHasProcessedAGoodbyeMessage : public ARelayStateWithAGoodbyeMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWithAGoodbyeMessage::SetUp();
        relay_state.ProcessGoodbyeMessage(goodbye, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWithAGoodbyeMessage::TearDown();
    }
};


class ARelayStateWhichHasProcessedADeath : public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    uint160 task_hash;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();

        AssignATaskToTheRelay();
        relay_state.RecordRelayDeath(relay, *data, MISBEHAVED);

    }

    void AssignATaskToTheRelay()
    {
        task_hash = 5;
        relay->tasks.push_back(Task(SEND_KEY_DISTRIBUTION_MESSAGE, task_hash, 1));
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

std::vector<std::pair<uint64_t, uint64_t> > GetQuarterHolderRoles(uint64_t relay_number, RelayState *state)
{
    std::vector<std::pair<uint64_t, uint64_t> > roles;

    for (uint64_t i = 0; i < state->relays.size(); i++)
        for (uint64_t j = 0; j < state->relays[i].key_quarter_holders.size(); j++)
            if (state->relays[i].key_quarter_holders[j] == relay_number)
                roles.push_back(std::make_pair(state->relays[i].number, j));

    return roles;
};

TEST_F(ARelayStateWhichHasProcessedADeath,
       ReplacesTheRelayWithItsSuccessorInAllQuarterHolderRolesWhenProcessingASuccessionCompletedMessage)
{
    uint64_t successor_number = relay->current_successor_number;

    auto initial_roles_of_exiting_relay = GetQuarterHolderRoles(relay->number, &relay_state);
    ASSERT_THAT(initial_roles_of_exiting_relay.size(), Gt(0));

    SuccessionCompletedMessage succession_completed_message;
    succession_completed_message.dead_relay_number = relay->number;
    succession_completed_message.successor_number = successor_number;

    relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);

    auto final_roles_of_exiting_relay = GetQuarterHolderRoles(relay->number, &relay_state);
    ASSERT_THAT(final_roles_of_exiting_relay.size(), Eq(0));

    auto final_roles_of_successor = GetQuarterHolderRoles(successor_number, &relay_state);
    for (auto role : initial_roles_of_exiting_relay)
        ASSERT_TRUE(VectorContainsEntry(final_roles_of_successor, role));
}

TEST_F(ARelayStateWhichHasProcessedADeath,
       MarksAKeyQuarterHolderAsNotRespondingAfterADurationWithoutAResponseFromThatRelay)
{
    DurationWithoutResponseFromRelay duration;
    duration.message_hash = Death(relay->number);
    duration.relay_number = relay->key_quarter_holders[1];

    relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);

    Relay *key_quarter_holder = relay_state.GetRelayByNumber(duration.relay_number);

    ASSERT_TRUE(RelayHasStatus(key_quarter_holder, *data, NOT_RESPONDING));
}


class ARelayStateWithASecretRecoveryMessage : public ARelayStateWhichHasProcessedADeath
{
public:
    Relay *key_quarter_holder{NULL};
    SecretRecoveryMessage secret_recovery_message;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedADeath::SetUp();

        secret_recovery_message = AValidSecretRecoveryMessage();
        data->StoreMessage(secret_recovery_message);
        key_quarter_holder = relay_state.GetRelayByNumber(secret_recovery_message.quarter_holder_number);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedADeath::TearDown();
    }

    SecretRecoveryMessage AValidSecretRecoveryMessage()
    {
        return ValidSecretRecoveryMessageForKeyQuarterHolder(1);
    }

    SecretRecoveryMessage ValidSecretRecoveryMessageForKeyQuarterHolder(uint32_t quarter_holder_position)
    {
        SecretRecoveryMessage secret_recovery_message;

        secret_recovery_message.quarter_holder_number = relay->key_quarter_holders[quarter_holder_position];
        secret_recovery_message.position_of_quarter_holder = (uint8_t) quarter_holder_position;
        secret_recovery_message.successor_number = relay->current_successor_number;
        secret_recovery_message.dead_relay_number = relay->number;
        secret_recovery_message.PopulateSecrets(*data);
        secret_recovery_message.Sign(*data);
        data->StoreMessage(secret_recovery_message);
        return secret_recovery_message;
    }

    vector<SecretRecoveryMessage> AllFourSecretRecoveryMessages()
    {
        vector<SecretRecoveryMessage> recovery_messages;
        for (uint32_t i = 0; i < 4; i++)
            recovery_messages.push_back(ValidSecretRecoveryMessageForKeyQuarterHolder(i));
        return recovery_messages;
    }
};

TEST_F(ARelayStateWithASecretRecoveryMessage, RecordsTheHashWhenProcessingTheSecretRecoveryMessage)
{
    relay_state.ProcessSecretRecoveryMessage(secret_recovery_message);
    auto &attempt = relay->succession_attempts[relay->current_successor_number];
    ASSERT_THAT(attempt.recovery_message_hashes[secret_recovery_message.position_of_quarter_holder],
                Eq(secret_recovery_message.GetHash160()));
}


class ARelayStateWhichHasProcessedASecretRecoveryMessage : public ARelayStateWithASecretRecoveryMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWithASecretRecoveryMessage::SetUp();
        relay_state.ProcessSecretRecoveryMessage(secret_recovery_message);
    }

    virtual void TearDown()
    {
        ARelayStateWithASecretRecoveryMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage,
       MarksTheKeyQuarterHolderAsMisbehavingWhenProcessingASecretRecoveryComplaint)
{
    SecretRecoveryComplaint complaint;
    complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();

    relay_state.ProcessSecretRecoveryComplaint(complaint, *data);

    ASSERT_TRUE(RelayHasStatus(key_quarter_holder, *data, MISBEHAVED));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage,
       RemovesTheKeyQuarterHoldersTask)
{
    bool still_there = VectorContainsEntry(key_quarter_holder->tasks,
                                           Task(SEND_SECRET_RECOVERY_MESSAGE, Death(relay->number), 1));
    ASSERT_FALSE(still_there);
}


//class ARelayStateWhichHasProcessedASecretRecoveryComplaint : public ARelayStateWhichHasProcessedASecretRecoveryMessage
//{
//public:
//    SecretRecoveryComplaint complaint;
//
//    virtual void SetUp()
//    {
//        ARelayStateWhichHasProcessedASecretRecoveryMessage::SetUp();
//
//        complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();
//        complaint.Sign(*data);
//        ASSERT_TRUE(complaint.VerifySignature(*data));
//
//        data->StoreMessage(complaint);
//
//        relay_state.ProcessSecretRecoveryComplaint(complaint, *data);
//    }
//
//    virtual void TearDown()
//    {
//        ARelayStateWhichHasProcessedASecretRecoveryMessage::TearDown();
//    }
//};

class ARelayStateWhichHasProcessedFourSecretRecoveryMessages : public ARelayStateWithASecretRecoveryMessage
{
public:
    std::array<uint160, 4> recovery_message_hashes;

    virtual void SetUp()
    {
        ARelayStateWithASecretRecoveryMessage::SetUp();
        auto recovery_messages = AllFourSecretRecoveryMessages();

        for (uint32_t i = 0; i < 4; i++)
        {
            relay_state.ProcessSecretRecoveryMessage(recovery_messages[i]);
            recovery_message_hashes[i] = recovery_messages[i].GetHash160();
        }
    }

    virtual void TearDown()
    {
        ARelayStateWithASecretRecoveryMessage::TearDown();
    }
};


class ARelayStateWithASecretRecoveryFailureMessage : public ARelayStateWhichHasProcessedFourSecretRecoveryMessages
{
public:
    SecretRecoveryFailureMessage failure_message;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedFourSecretRecoveryMessages::SetUp();

        failure_message.recovery_message_hashes = recovery_message_hashes;
        failure_message.key_sharer_position = 0;
        failure_message.shared_secret_quarter_position = 0;
        failure_message.sum_of_decrypted_shared_secret_quarters = Point(CBigNum(2));
        failure_message.Populate(recovery_message_hashes, *data);
        failure_message.Sign(*data);
        data->StoreMessage(failure_message);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedFourSecretRecoveryMessages::TearDown();
    }
};

TEST_F(ARelayStateWithASecretRecoveryFailureMessage, AssignsTheFailureMessageAsATaskToTheKeyQuarterHolders)
{
    relay_state.ProcessSecretRecoveryFailureMessage(failure_message, *data);
    for (uint32_t i = 0; i < 4; i++)
    {
        auto quarter_holder = relay->QuarterHolders(*data)[i];
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks,
                                        Task(SEND_RECOVERY_FAILURE_AUDIT_MESSAGE, failure_message.GetHash160(), i)));
    }
}

TEST_F(ARelayStateWithASecretRecoveryFailureMessage, MarksTheRelaysThatDontRespondAsNotResponding)
{
    relay_state.ProcessSecretRecoveryFailureMessage(failure_message, *data);
    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = failure_message.GetHash160();
        duration.relay_number = quarter_holder->number;
        relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);
        ASSERT_THAT(quarter_holder->status, Eq(NOT_RESPONDING));
    }
}


class ARelayStateWithARecoveryFailureAuditMessage : public ARelayStateWithASecretRecoveryFailureMessage
{
public:
    RecoveryFailureAuditMessage audit_message;

    virtual void SetUp()
    {
        ARelayStateWithASecretRecoveryFailureMessage::SetUp();
        relay_state.ProcessSecretRecoveryFailureMessage(failure_message, *data);
        SecretRecoveryMessage recovery_message = data->GetMessage(failure_message.recovery_message_hashes[0]);
        audit_message.Populate(failure_message, recovery_message, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWithASecretRecoveryFailureMessage::TearDown();
    }
};

TEST_F(ARelayStateWithARecoveryFailureAuditMessage, RemovesTheFailureMessageFromTheQuarterHoldersTasks)
{
    auto quarter_holder = relay_state.GetRelayByNumber(relay->key_quarter_holders[0]);
    ASSERT_THAT(quarter_holder->tasks.size(), Eq(1));
    relay_state.ProcessRecoveryFailureAuditMessage(audit_message, *data);
    ASSERT_THAT(quarter_holder->tasks.size(), Eq(0));
}