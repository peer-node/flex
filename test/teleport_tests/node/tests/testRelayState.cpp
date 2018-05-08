#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"
#include "test/teleport_tests/node/relays/structures/RelayMemoryCache.h"

using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class ARelayState : public Test
{
public:
    RelayState relay_state;
    RelayJoinMessage relay_join_message;
    MemoryDataStore keydata, creditdata, msgdata, depositdata;
    Data *data;

    virtual void SetUp()
    {
        data = new Data(msgdata, creditdata, keydata, depositdata, &relay_state);
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
            log_ << "set up relay state for first time: number of relays is " << relay_state.relays.size() << "\n";
        }
        else
        {
            relay_state = reference_test_relay_state;
            keydata = reference_test_keydata;
            msgdata = reference_msgdata;
            log_ << "used stored relays state: number of relays is " << relay_state.relays.size() << "\n";
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
    sleep(10);
    for (auto &relay : relay_state.relays)
    {
        log_ << "checking holders assigned for " << relay.number << "\n";
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

TEST_F(ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage,
       AssignsANewKeyDistributionTaskWhenProcessingAMinedCreditMessageAfterAQuarterHolderWithNoSuccessorHasDied)
{
    relay_state.ProcessMinedCreditMessage(msg, *data);

    auto relay = relay_state.GetRelayByNumber(4);

    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, 1, relay_state);
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);

    auto quarter_holder = relay->QuarterHolders(*data)[0];
    quarter_holder->current_successor_number = 0;
    relay_state.RecordRelayDeath(quarter_holder, *data, SAID_GOODBYE);

    MinedCreditMessage msg_;
    relay_state.ProcessMinedCreditMessage(msg_, *data);

    Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay->hashes.join_message_hash, 0);
    ASSERT_TRUE(VectorContainsEntry(relay->tasks, task));
}

TEST_F(ARelayStateWith53RelaysWithRelayJoinMessagesEncodedInAMinedCreditMessage,
       RemovesAKeyDistributionTaskWithTheSpecifiedPositionWhenProcessingAKeyDistributionMessage)
{
    auto relay = relay_state.GetRelayByNumber(4);
    Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay->hashes.join_message_hash, 0);

    relay->tasks.push_back(task);

    auto key_distribution_message = relay->GenerateKeyDistributionMessage(*data, 1, relay_state, 0);
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);

    ASSERT_FALSE(VectorContainsEntry(relay->tasks, task));
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

        number_of_chosen_relay = TheNumberOfARelayWhichIsBeingUsedAsAKeyQuarterHolder();
        relay = relay_state.GetRelayByNumber(number_of_chosen_relay);
        DoCachedSetUpForARelayStateWith53RelaysAndAKeyDistributionMessage();
        data->StoreMessage(key_distribution_message);
    }

    virtual void DoCachedSetUpForARelayStateWith53RelaysAndAKeyDistributionMessage()
    {
        static RelayState reference_test_relay_state;
        static KeyDistributionMessage reference_key_distribution_message;

        if (reference_test_relay_state.relays.size() == 0)
        {
            key_distribution_message = relay->GenerateKeyDistributionMessage(*data, 5, relay_state);
            reference_test_relay_state = relay_state;
            reference_key_distribution_message = key_distribution_message;
        }
        else
        {
            relay_state = reference_test_relay_state;
            key_distribution_message = reference_key_distribution_message;
        }
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

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage, MarksTheRelayAsHavingSaidGoodbye)
{
    ASSERT_THAT(relay->status, Eq(SAID_GOODBYE));
}


class ARelayStateWhichHasProcessedADeath : public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    uint160 task_hash;
    Relay *successor;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();

        AssignATaskToTheRelay();
        relay_state.RecordRelayDeath(relay, *data, MISBEHAVED);
        successor = relay_state.GetRelayByNumber(relay->current_successor_number);
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
    auto initial_roles_of_exiting_relay = GetQuarterHolderRoles(relay->number, &relay_state);
    ASSERT_THAT(initial_roles_of_exiting_relay.size(), Gt(0));

    SuccessionCompletedMessage succession_completed_message;
    succession_completed_message.dead_relay_number = relay->number;
    succession_completed_message.successor_number = successor->number;

    relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);

    auto final_roles_of_exiting_relay = GetQuarterHolderRoles(relay->number, &relay_state);
    ASSERT_THAT(final_roles_of_exiting_relay.size(), Eq(0));

    auto final_roles_of_successor = GetQuarterHolderRoles(successor->number, &relay_state);
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

    ASSERT_THAT(key_quarter_holder->status, Eq(NOT_RESPONDING));
}

class ARelayStateWithARelayWithAKeyQuarterHolderWhoHasDiedWithoutASuccessor :
        public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    uint160 task_hash;
    Relay *quarter_holder;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();

        quarter_holder = relay->QuarterHolders(*data)[0];
        quarter_holder->current_successor_number = 0;
        relay_state.RecordRelayDeath(quarter_holder, *data, SAID_GOODBYE);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayStateWithARelayWithAKeyQuarterHolderWhoHasDiedWithoutASuccessor, AssignsANewKeyQuarterHolderToTheRelay)
{
    auto new_quarter_holder_number = relay->QuarterHolders(*data)[0]->number;
    ASSERT_THAT(new_quarter_holder_number, Ne(quarter_holder->number));
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
        secret_recovery_message.successor_number = successor->number;
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
    auto &attempt = relay->succession_attempts[successor->number];
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

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage, RemovesTheKeyQuarterHoldersTask)
{
    bool still_there = VectorContainsEntry(key_quarter_holder->tasks,
                                           Task(SEND_SECRET_RECOVERY_MESSAGE, Death(relay->number), 1));
    ASSERT_FALSE(still_there);
}


class ARelayStateWithASecretRecoveryComplaint : public ARelayStateWhichHasProcessedASecretRecoveryMessage
{
public:
    SecretRecoveryComplaint complaint;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedASecretRecoveryMessage::SetUp();
        complaint.successor_number = successor->number;
        complaint.position_of_quarter_holder = secret_recovery_message.position_of_quarter_holder;
        complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();
        data->StoreMessage(complaint);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedASecretRecoveryMessage::TearDown();
    }
};

TEST_F(ARelayStateWithASecretRecoveryComplaint, IncrementsTheNumberOfComplaintsSentByTheSuccessorWhenProcessingIt)
{
    ASSERT_THAT(successor->number_of_complaints_sent, Eq(0));
    relay_state.ProcessSecretRecoveryComplaint(complaint, *data);
    ASSERT_THAT(successor->number_of_complaints_sent, Eq(1));
}


class ARelayStateWhichHasProcessedASecretRecoveryComplaint : public ARelayStateWithASecretRecoveryComplaint
{
public:
    virtual void SetUp()
    {
        ARelayStateWithASecretRecoveryComplaint::SetUp();
        relay_state.ProcessSecretRecoveryComplaint(complaint, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWithASecretRecoveryComplaint::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint, RecordsTheHashOfTheComplaint)
{
    auto &attempt = relay->succession_attempts[successor->number];
    ASSERT_TRUE(VectorContainsEntry(attempt.recovery_complaint_hashes, complaint.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint,
       MovesTheHashOfTheBadSecretRecoveryMessageToTheListOfRejectedMessages)
{
    auto &attempt = relay->succession_attempts[successor->number];
    ASSERT_TRUE(VectorContainsEntry(attempt.rejected_recovery_messages, secret_recovery_message.GetHash160()));
    ASSERT_FALSE(ArrayContainsEntry(attempt.recovery_message_hashes, secret_recovery_message.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint, MarksTheKeyQuarterHolderAsMisbehaving)
{
    auto quarter_holder = complaint.GetSecretSender(*data);
    ASSERT_THAT(key_quarter_holder->status, Eq(MISBEHAVED));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint,
       ReassignsTheSendSecretRecoveryMessageTaskToTheMisbehavingQuarterHolder)
{
    auto quarter_holder = complaint.GetSecretSender(*data);
    Task task(SEND_SECRET_RECOVERY_MESSAGE, Death(relay->number), complaint.position_of_quarter_holder);
    ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, task));
}


class ARelayStateWhichHasProcessedFourSecretRecoveryMessages : public ARelayStateWithASecretRecoveryMessage
{
public:
    std::array<uint160, 4> recovery_message_hashes;
    FourSecretRecoveryMessages four_recovery_messages;

    virtual void SetUp()
    {
        ARelayStateWithASecretRecoveryMessage::SetUp();
        auto recovery_messages = AllFourSecretRecoveryMessages();

        successor = relay_state.GetRelayByNumber(relay->current_successor_number);

        for (uint32_t i = 0; i < 4; i++)
        {
            relay_state.ProcessSecretRecoveryMessage(recovery_messages[i]);
            recovery_message_hashes[i] = recovery_messages[i].GetHash160();
        }
        auto &attempt = relay->succession_attempts.begin()->second;
        four_recovery_messages = attempt.GetFourSecretRecoveryMessages();
    }

    virtual void TearDown()
    {
        ARelayStateWithASecretRecoveryMessage::TearDown();
    }

    SecretRecoveryComplaint ComplaintAboutOneOfTheSecretRecoveryMessages()
    {
        SecretRecoveryComplaint complaint;
        complaint.successor_number = successor->number;
        complaint.position_of_quarter_holder = secret_recovery_message.position_of_quarter_holder;
        complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();
        data->StoreMessage(complaint);
        return complaint;
    }
};

TEST_F(ARelayStateWhichHasProcessedFourSecretRecoveryMessages, AssignsACompleteSuccessionTaskToTheSuccessor)
{
    Task task(SEND_SUCCESSION_COMPLETED_MESSAGE, four_recovery_messages.GetHash160());
    ASSERT_TRUE(VectorContainsEntry(successor->tasks, task));
}

TEST_F(ARelayStateWhichHasProcessedFourSecretRecoveryMessages,
       RemovesTheCompleteSuccessionTaskFromTheSuccessorWhenProcessingASecretRecoveryComplaint)
{
    Task task(SEND_SUCCESSION_COMPLETED_MESSAGE, four_recovery_messages.GetHash160());
    auto complaint = ComplaintAboutOneOfTheSecretRecoveryMessages();
    relay_state.ProcessSecretRecoveryComplaint(complaint, *data);
    ASSERT_FALSE(VectorContainsEntry(successor->tasks, task));
}


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

TEST_F(ARelayStateWithARecoveryFailureAuditMessage, RemovesTheSendAuditMessageTasksFromTheQuarterHolders)
{
    auto quarter_holder = relay_state.GetRelayByNumber(relay->key_quarter_holders[0]);
    ASSERT_THAT(quarter_holder->tasks.size(), Eq(1));
    relay_state.ProcessRecoveryFailureAuditMessage(audit_message, *data);
    ASSERT_THAT(quarter_holder->tasks.size(), Eq(0));
}


class ARelayStateWithASuccessionCompletedMessage : public ARelayStateWhichHasProcessedFourSecretRecoveryMessages
{
public:
    SuccessionCompletedMessage succession_completed_message;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedFourSecretRecoveryMessages::SetUp();

        succession_completed_message.dead_relay_number = relay->number;
        succession_completed_message.successor_number = relay->current_successor_number;
        auto &attempt = relay->succession_attempts.begin()->second;
        succession_completed_message.recovery_message_hashes = attempt.recovery_message_hashes;
        SecretRecoveryMessage recovery_message = data->GetMessage(attempt.recovery_message_hashes[0]);
        succession_completed_message.key_quarter_sharers = recovery_message.key_quarter_sharers;
        succession_completed_message.key_quarter_positions = recovery_message.key_quarter_positions;

        data->StoreMessage(succession_completed_message);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedFourSecretRecoveryMessages::TearDown();
    }
};

TEST_F(ARelayStateWithASuccessionCompletedMessage, RecordsTheHashOfTheSuccessionCompletedMessageWhenProcessingIt)
{
    relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);
    auto &attempt = relay->succession_attempts.begin()->second;
    ASSERT_THAT(attempt.succession_completed_message_hash, Eq(succession_completed_message.GetHash160()));
}

TEST_F(ARelayStateWithASuccessionCompletedMessage, RemovesTheCompleteSuccessionTaskFromTheSuccessor)
{
    Task task(SEND_SUCCESSION_COMPLETED_MESSAGE, four_recovery_messages.GetHash160());
    ASSERT_TRUE(VectorContainsEntry(successor->tasks, task));
    relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);
    ASSERT_FALSE(VectorContainsEntry(successor->tasks, task));
}

TEST_F(ARelayStateWithASuccessionCompletedMessage, AssignsTheDeadRelaysInheritableTasksToTheSuccessor)
{
    Task inheritable_task(SEND_SECRET_RECOVERY_MESSAGE, 1);
    relay->tasks.push_back(inheritable_task);
    relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);
    ASSERT_TRUE(VectorContainsEntry(successor->tasks, inheritable_task));
}

TEST_F(ARelayStateWithASuccessionCompletedMessage, DoesntAssignTheDeadRelaysNonInheritableTasksToTheSuccessor)
{
    Task non_inheritable_task(SEND_KEY_DISTRIBUTION_MESSAGE, 1);
    relay->tasks.push_back(non_inheritable_task);
    relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);
    ASSERT_FALSE(VectorContainsEntry(successor->tasks, non_inheritable_task));
}


class ARelayStateWhichHasProcessedASuccessionCompletedMessage : public ARelayStateWithASuccessionCompletedMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWithASuccessionCompletedMessage::SetUp();
        relay_state.ProcessSuccessionCompletedMessage(succession_completed_message, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWithASuccessionCompletedMessage::TearDown();
    }
};


class ARelayStateWithAMinedCreditMessageContainingASuccessionCompletedMessage :
        public ARelayStateWhichHasProcessedASuccessionCompletedMessage
{
public:
    MinedCreditMessage msg;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedASuccessionCompletedMessage::SetUp();

        msg.hash_list.full_hashes.push_back(succession_completed_message.GetHash160());
        msg.hash_list.GenerateShortHashes();
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedASuccessionCompletedMessage::TearDown();
    }
};

class ARelayStateWhichHasProcessedAMinedCreditMessageContainingASuccessionCompletedMessage :
        public ARelayStateWithAMinedCreditMessageContainingASuccessionCompletedMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWithAMinedCreditMessageContainingASuccessionCompletedMessage::SetUp();
        relay_state.ProcessMinedCreditMessage(msg, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWithAMinedCreditMessageContainingASuccessionCompletedMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedAMinedCreditMessageContainingASuccessionCompletedMessage,
       MarksTheSuccessionCompletedMessageAsEncodedInTheMinedCreditMessage)
{
    auto &attempt = relay->succession_attempts[successor->number];
    ASSERT_THAT(attempt.hash_of_message_containing_succession_completed_message, Eq(msg.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAMinedCreditMessageContainingASuccessionCompletedMessage,
       AssignsASuccesionCompletedAuditTaskToTheSuccessor)
{
    Task task(SEND_SUCCESSION_COMPLETED_AUDIT_MESSAGE, succession_completed_message.GetHash160());
    ASSERT_TRUE(VectorContainsEntry(successor->tasks, task));
}


class ARelayStateWithASuccessionCompletedAuditMessage :
        public ARelayStateWhichHasProcessedAMinedCreditMessageContainingASuccessionCompletedMessage
{
public:
    SuccessionCompletedAuditMessage audit_message;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAMinedCreditMessageContainingASuccessionCompletedMessage::SetUp();

        audit_message.dead_relay_number = relay->number;
        audit_message.successor_number = successor->number;
        audit_message.succession_completed_message_hash = succession_completed_message.GetHash160();
        data->StoreMessage(audit_message);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAMinedCreditMessageContainingASuccessionCompletedMessage::TearDown();
    }
};

TEST_F(ARelayStateWithASuccessionCompletedAuditMessage, RecordsTheHashOfTheAuditMessage)
{
    relay_state.ProcessSuccessionCompletedAuditMessage(audit_message, *data);
    auto &attempt = relay->succession_attempts[successor->number];
    ASSERT_THAT(attempt.succession_completed_audit_message_hash, Eq(audit_message.GetHash160()));
}

TEST_F(ARelayStateWithASuccessionCompletedAuditMessage, RemovesTheSuccessionCompletedAuditTaskFromTheSuccessor)
{
    relay_state.ProcessSuccessionCompletedAuditMessage(audit_message, *data);
    Task task(SEND_SUCCESSION_COMPLETED_AUDIT_MESSAGE, succession_completed_message.GetHash160());
    ASSERT_TRUE(VectorContainsEntry(successor->tasks, task));
}


class ARelayStateWhichHasProcessedASuccessionCompletedAuditMessage : public ARelayStateWithASuccessionCompletedAuditMessage
{
public:
    Relay *key_quarter_sharer;
    uint8_t key_quarter_position;

    virtual void SetUp()
    {
        ARelayStateWithASuccessionCompletedAuditMessage::SetUp();
        relay_state.ProcessSuccessionCompletedAuditMessage(audit_message, *data);

        uint64_t key_quarter_sharer_number = succession_completed_message.key_quarter_sharers[0];
        key_quarter_sharer = relay_state.GetRelayByNumber(key_quarter_sharer_number);
        key_quarter_position = succession_completed_message.key_quarter_positions[0];
    }

    virtual void TearDown()
    {
        ARelayStateWithASuccessionCompletedAuditMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedASuccessionCompletedAuditMessage,
       SetsTheSuccessionCompletedMessageAsTheLocationForKeyQuartersAfterADurationWithoutAResponse)
{
    auto duration = GetDurationWithoutResponseAfter(audit_message.GetHash160());
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    auto &location = key_quarter_sharer->key_quarter_locations[key_quarter_position];

    ASSERT_THAT(location.message_hash, Eq(succession_completed_message.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedASuccessionCompletedAuditMessage,
       RecordsThePreviousLocationsOfTheQuarterHoldersAfterADurationWithoutAResponse)
{
    uint160 key_distribution_message_hash = key_quarter_sharer->key_quarter_locations[0].message_hash;

    DurationWithoutResponse duration = GetDurationWithoutResponseAfter(audit_message.GetHash160());
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    ASSERT_THAT(key_quarter_sharer->previous_key_quarter_locations.size(), Gt(0));
    ASSERT_THAT(key_quarter_sharer->previous_key_quarter_locations.back(), Eq(key_distribution_message_hash));
}


class ARelayStateWithASuccessionCompletedAuditComplaint :
        public ARelayStateWhichHasProcessedASuccessionCompletedAuditMessage
{
public:
    SuccessionCompletedAuditComplaint complaint;
    Relay *auditor;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedASuccessionCompletedAuditMessage::SetUp();

        complaint.succession_completed_message_hash = succession_completed_message.GetHash160();
        complaint.succession_completed_audit_message_hash = audit_message.GetHash160();
        complaint.successor_number = successor->number;
        complaint.dead_relay_number = relay->number;
        complaint.auditor_number = 22;
        data->StoreMessage(complaint);

        auditor = relay_state.GetRelayByNumber(complaint.auditor_number);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedASuccessionCompletedAuditMessage::TearDown();
    }
};

TEST_F(ARelayStateWithASuccessionCompletedAuditComplaint,
       IncrementsTheNumberOfComplaintsSentByTheAuditorWhenProcessingTheComplaint)
{
    auto initial_number_of_complaints = auditor->number_of_complaints_sent;
    relay_state.ProcessSuccessionCompletedAuditComplaint(complaint, *data);
    ASSERT_THAT(auditor->number_of_complaints_sent, Eq(initial_number_of_complaints + 1));
}

TEST_F(ARelayStateWithASuccessionCompletedAuditComplaint,
       RestoresThePreviousKeyQuarterLocationsIfTheComplaintArrivesAfterADurationWithoutAResponse)
{
    uint160 key_distribution_message_hash = key_quarter_sharer->key_quarter_locations[0].message_hash;

    auto duration = GetDurationWithoutResponseAfter(audit_message.GetHash160());
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    relay_state.ProcessSuccessionCompletedAuditComplaint(complaint, *data);

    auto &location = key_quarter_sharer->key_quarter_locations[key_quarter_position];

    ASSERT_THAT(location.message_hash, Eq(key_distribution_message_hash));
    ASSERT_THAT(location.position, Eq(key_quarter_position));
    ASSERT_THAT(key_quarter_sharer->previous_key_quarter_locations.size(), Eq(0));
}


class ARelayStateWithASuccessionCompletedAuditComplaintWithARelayWhoseKeyQuarterIsLocatedInASuccessionCompletedMessage :
        public ARelayStateWithASuccessionCompletedAuditComplaint
{
public:
    SuccessionCompletedMessage succession_completed1;

    virtual void SetUp()
    {
        ARelayStateWithASuccessionCompletedAuditComplaint::SetUp();

        succession_completed1 = SuccessionCompletedMessageWithKeyQuarterInPosition1();
        key_quarter_sharer->key_quarter_locations[1].message_hash = succession_completed1.GetHash160();
        key_quarter_sharer->key_quarter_locations[1].position = 0;
    }

    SuccessionCompletedMessage SuccessionCompletedMessageWithKeyQuarterInPosition1()
    {
        SuccessionCompletedMessage succession_completed;
        succession_completed.key_quarter_sharers.push_back(2);
        succession_completed.key_quarter_positions.push_back(0);
        succession_completed.key_quarter_sharers.push_back(key_quarter_sharer->number);
        succession_completed.key_quarter_positions.push_back(1);
        data->StoreMessage(succession_completed);
        return succession_completed;
    }

    virtual void TearDown()
    {
        ARelayStateWithASuccessionCompletedAuditComplaint::TearDown();
    }
};

TEST_F(ARelayStateWithASuccessionCompletedAuditComplaintWithARelayWhoseKeyQuarterIsLocatedInASuccessionCompletedMessage,
       RestoresThePreviousKeyQuarterLocationsIfTheComplaintArrivesAfterADurationWithoutAResponse)
{
    uint160 succession_completed1_hash = succession_completed1.GetHash160();

    auto duration = GetDurationWithoutResponseAfter(audit_message.GetHash160());
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    relay_state.ProcessSuccessionCompletedAuditComplaint(complaint, *data);

    auto &location = key_quarter_sharer->key_quarter_locations[key_quarter_position];

    ASSERT_THAT(location.message_hash, Eq(succession_completed1_hash));
    ASSERT_THAT(location.position, Eq(1));
    ASSERT_THAT(key_quarter_sharer->previous_key_quarter_locations.size(), Eq(0));
}


class ARelayStateWhichHasProcessedASuccessionCompletedAuditComplaint :
        public ARelayStateWithASuccessionCompletedAuditComplaint
{
public:
    virtual void SetUp()
    {
        ARelayStateWithASuccessionCompletedAuditComplaint::SetUp();
        relay_state.ProcessSuccessionCompletedAuditComplaint(complaint, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWithASuccessionCompletedAuditComplaint::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedASuccessionCompletedAuditComplaint, RecordsTheHashOfTheComplaint)
{
    auto &attempt = relay->succession_attempts[successor->number];
    ASSERT_TRUE(VectorContainsEntry(attempt.succession_completed_audit_complaints, complaint.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedASuccessionCompletedAuditComplaint, MarksTheSuccessorAsMisbehaving)
{
    ASSERT_THAT(successor->status, Eq(MISBEHAVED));
}

TEST_F(ARelayStateWhichHasProcessedASuccessionCompletedAuditComplaint, AssignsANewSuccessorToTheDeadRelay)
{
    ASSERT_THAT(relay->current_successor_number, Ne(successor->number));
}

TEST_F(ARelayStateWhichHasProcessedASuccessionCompletedAuditComplaint,
       ReassignsTheSecretRecoveryTasksToTheQuarterHolders)
{
    for (uint32_t position = 0; position < 4; position++)
    {
        auto quarter_holder = relay->QuarterHolders(*data)[position];
        Task task(SEND_SECRET_RECOVERY_MESSAGE, Death(relay->number), position);
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, task));
    }
}