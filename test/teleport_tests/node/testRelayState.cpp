#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relay_handler/RelayState.h"
#include "test/teleport_tests/node/relay_handler/SecretRecoveryComplaint.h"

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

TEST_F(ARelayState, ThrowsAnExceptionIfTheSameMinedCreditMessageHashIsUsedTwiceInJoinMessages)
{
    RelayJoinMessage relay_join_message;
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    EXPECT_THROW(relay_state.ProcessRelayJoinMessage(relay_join_message), RelayStateException);
}


class ARelayStateWith37Relays : public ARelayState
{
public:
    virtual void SetUp()
    {
        ARelayState::SetUp();
        uint64_t start = GetTimeMicros();
        static RelayState reference_test_relay_state;
        static MemoryDataStore reference_test_keydata;

        if (reference_test_relay_state.relays.size() == 0)
        {
            for (uint32_t i = 1; i <= 37; i++)
            {
                RelayJoinMessage join_message = Relay().GenerateJoinMessage(keydata, i);
                relay_state.ProcessRelayJoinMessage(join_message);
            }
            reference_test_relay_state = relay_state;
            reference_test_keydata = keydata;
        }
        else
        {
            relay_state = reference_test_relay_state;
            keydata = reference_test_keydata;
        }
    }

    virtual void TearDown()
    {
        ARelayState::TearDown();
    }
};

bool VectorContainsANumberGreaterThan(std::vector<uint64_t> numbers, uint64_t given_number)
{
    for (auto number : numbers)
        if (number > given_number)
            return true;
    return false;
}

TEST_F(ARelayStateWith37Relays, AssignsKeyPartHoldersToEachRelayWithThreeRelaysWhoJoinedLaterThanIt)
{
    uint160 encoding_message_hash = 1;
    for (auto relay : relay_state.relays)
    {
        bool holders_assigned = relay_state.AssignKeyPartHoldersToRelay(relay, encoding_message_hash);
        ASSERT_THAT(holders_assigned, Eq(relay.number <= 34));
    }
}

TEST_F(ARelayStateWith37Relays, AssignsFourQuarterKeyHoldersAtLeastOneOfWhichJoinedLaterThanTheGivenRelay)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_THAT(relay.holders.key_quarter_holders.size(), Eq(4));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.holders.key_quarter_holders, relay.number));
        }
    }
}

TEST_F(ARelayStateWith37Relays, AssignsKeySixteenthHoldersAtLeastOneOfWhichJoinedLaterThanTheGivenRelay)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_THAT(relay.holders.first_set_of_key_sixteenth_holders.size(), Eq(16));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.holders.first_set_of_key_sixteenth_holders, relay.number));
            ASSERT_THAT(relay.holders.second_set_of_key_sixteenth_holders.size(), Eq(16));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.holders.second_set_of_key_sixteenth_holders, relay.number));
        }
    }
}

bool RelayHasDistinctKeyPartHolders(Relay &relay)
{
    std::set<uint64_t> key_part_holders;
    for (auto key_holder_group : relay.KeyPartHolderGroups())
        for (auto key_part_holder : key_holder_group)
            if (key_part_holders.count(key_part_holder))
                return false;
            else
                key_part_holders.insert(key_part_holder);
    return true;
}

TEST_F(ARelayStateWith37Relays, Assigns36DistinctKeyPartHolders)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_TRUE(RelayHasDistinctKeyPartHolders(relay));
        }
    }
}

TEST_F(ARelayStateWith37Relays, DoesntAssignARelayToHoldItsOwnKeyParts)
{
    for (Relay &relay : relay_state.relays)
    {
        relay_state.AssignKeyPartHoldersToRelay(relay, 1);
        for (auto key_holder_group : relay.KeyPartHolderGroups())
            ASSERT_THAT(VectorContainsEntry(key_holder_group, relay.number), Eq(false));
    }
}

TEST_F(ARelayStateWith37Relays, DoesntAssignTwoRelaysAsEachOthersKeyQuarterHolders)
{
    for (Relay &relay : relay_state.relays)
        relay_state.AssignKeyPartHoldersToRelay(relay, relay.number);

    for (Relay &first_relay : relay_state.relays)
        for (Relay &second_relay : relay_state.relays)
        {
            bool first_has_second_as_quarter_holder = VectorContainsEntry(first_relay.holders.key_quarter_holders,
                                                                          second_relay.number);
            bool second_has_first_as_quarter_holder = VectorContainsEntry(second_relay.holders.key_quarter_holders,
                                                                          first_relay.number);
            ASSERT_FALSE(first_has_second_as_quarter_holder and second_has_first_as_quarter_holder);
        }
}

class ARelayStateWith37RelaysAndAKeyDistributionMessage : public ARelayStateWith37Relays
{
public:
    Relay *relay;
    KeyDistributionMessage key_distribution_message;

    virtual void SetUp()
    {
        ARelayStateWith37Relays::SetUp();
        SetUpRelayState();

        relay = relay_state.GetRelayByNumber(TheNumberOfARelayWhichIsBeingUsedAsAKeyQuarterHolder());
        key_distribution_message = relay->GenerateKeyDistributionMessage(*data, 5, relay_state);
    }

    virtual void SetUpRelayState()
    {
        static RelayState reference_test_relay_state;

        if (reference_test_relay_state.relays.size() == 0)
        {
            uint160 encoding_message_hash = 1;
            for (Relay& key_sharer : relay_state.relays)
                relay_state.AssignKeyPartHoldersToRelay(key_sharer, encoding_message_hash++);
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
        return relay_state.relays[0].holders.key_quarter_holders[1];
    }

    virtual void TearDown()
    {
        ARelayStateWith37Relays::TearDown();
    }
};

TEST_F(ARelayStateWith37RelaysAndAKeyDistributionMessage,
       RecordsTheRelaysKeyDistributionMessageHashWhenProcessingAKeyDistributionMessage)
{;
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    ASSERT_THAT(relay->hashes.key_distribution_message_hash, Eq(key_distribution_message.GetHash160()));
}

TEST_F(ARelayStateWith37RelaysAndAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToProcessAKeyDistributionMessageFromARelayWhichHasAlreadySentOne)
{
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    EXPECT_THROW(relay_state.ProcessKeyDistributionMessage(key_distribution_message), RelayStateException);
}


class ARelayStateWhichHasProcessedAKeyDistributionMessage : public ARelayStateWith37RelaysAndAKeyDistributionMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWith37RelaysAndAKeyDistributionMessage::SetUp();
        relay_state.ProcessKeyDistributionMessage(key_distribution_message);
        data->StoreMessage(key_distribution_message);
    }

    virtual void TearDown()
    {
        ARelayStateWith37RelaysAndAKeyDistributionMessage::TearDown();
    }
};

bool RelayHasAnObituaryWithSpecificReasonForLeaving(Relay *relay, Data data, uint32_t reason)
{
    if (relay->hashes.obituary_hash == 0)
        return false;
    std::string message_type = data.msgdata[relay->hashes.obituary_hash]["type"];
    if (message_type != "obituary")
        return false;
    Obituary obituary = data.msgdata[relay->hashes.obituary_hash]["obituary"];
    return obituary.dead_relay_number == relay->number and obituary.reason_for_leaving == reason;
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       GeneratesAnObituaryForTheRelayWhoWasComplainedAboutWhenProcessingAKeyDistributionComplaint)
{
    uint64_t set_of_secrets = KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS;
    uint64_t position = 2;
    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(), set_of_secrets, position, *data);

    relay_state.ProcessKeyDistributionComplaint(complaint, *data);

    Relay *complainer = complaint.GetComplainer(*data);
    Relay *secret_sender = complaint.GetSecretSender(*data);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(secret_sender, *data, OBITUARY_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage, AcceptsTheMessageAfterADurationWithoutAResponse)
{
    ASSERT_THAT(relay->key_distribution_message_accepted, Eq(false));

    DurationWithoutResponse duration_after_key_distribution_message;
    duration_after_key_distribution_message.message_hash = key_distribution_message.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration_after_key_distribution_message, *data);

    ASSERT_THAT(relay->key_distribution_message_accepted, Eq(true));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToProcessAComplaintAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration_after_key_distribution_message;
    duration_after_key_distribution_message.message_hash = key_distribution_message.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration_after_key_distribution_message, *data);

    KeyDistributionComplaint complaint;
    complaint.Populate(key_distribution_message.GetHash160(),
                       KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS, 2, *data);
    EXPECT_THROW(relay_state.ProcessKeyDistributionComplaint(complaint, *data), RelayStateException);
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       GeneratesAnObituaryWithARelayInGoodStandingIfTheRelayIsOldEnoughAndSaidGoodbye)
{
    relay_state.relays.back().number = 10000; // age is determined by difference between given and latest relay numbers
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    ASSERT_THAT(obituary.in_good_standing, Eq(true));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       GeneratesAnObituaryWithARelayNotInGoodStandingIfTheRelayIsNotOldEnoughOrDidntSayGoodbye)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    ASSERT_THAT(obituary.in_good_standing, Eq(false));
    relay_state.relays.back().number = 10000;
    obituary = relay_state.GenerateObituary(relay, OBITUARY_NOT_RESPONDING);
    ASSERT_THAT(obituary.in_good_standing, Eq(false));
    obituary = relay_state.GenerateObituary(relay, OBITUARY_COMPLAINT);
    ASSERT_THAT(obituary.in_good_standing, Eq(false));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       GeneratesAnObituaryWhichSpecifiesTheDeadRelaysSuccessor)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    ASSERT_THAT(obituary.successor_number, Eq(relay_state.AssignSuccessorToRelay(relay)));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       RecordsARelaysObituaryHashWhenProcessingTheObituary)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    relay_state.ProcessObituary(obituary);
    ASSERT_THAT(relay->hashes.obituary_hash, Eq(obituary.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsTheObituaryAsATaskForTheKeyQuarterHoldersWhenProcessingItIfTheRelayDidntSayGoodbyeCorrectly)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_COMPLAINT);
    relay_state.ProcessObituary(obituary);
    for (auto key_quarter_holder_relay_number : relay->holders.key_quarter_holders)
    {
        auto key_quarter_holder = relay_state.GetRelayByNumber(key_quarter_holder_relay_number);
        ASSERT_TRUE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
    }
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       DoesntAssignTheObituaryAsATaskForTheKeyQuarterHoldersWhenProcessingItIfTheRelayDidSayGoodbyeCorrectly)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    relay_state.ProcessObituary(obituary);
    for (auto key_quarter_holder_relay_number : relay->holders.key_quarter_holders)
    {
        auto key_quarter_holder = relay_state.GetRelayByNumber(key_quarter_holder_relay_number);
        ASSERT_FALSE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
    }
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsADistinctSuccessorWhichIsNotAKeyQuarterHolderToARelayWhichIsExiting)
{
    uint64_t successor_relay_number = relay_state.AssignSuccessorToRelay(relay);
    ASSERT_FALSE(VectorContainsEntry(relay->holders.key_quarter_holders, successor_relay_number));
    ASSERT_THAT(relay->number, Ne(successor_relay_number));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsASuccessorSuchThatNoTwoRelaysAreEachOthersKeyQuarterHolders)
{
    uint64_t successor_relay_number = relay_state.AssignSuccessorToRelay(relay);

    for (Relay &key_sharer : relay_state.relays)
        if (VectorContainsEntry(key_sharer.holders.key_quarter_holders, relay->number))
        {
            EraseEntryFromVector(relay->number, key_sharer.holders.key_quarter_holders);
            key_sharer.holders.key_quarter_holders.push_back(successor_relay_number);
        }

    for (Relay &first_relay : relay_state.relays)
        for (Relay &second_relay : relay_state.relays)
        {
            bool first_has_second_as_quarter_holder = VectorContainsEntry(first_relay.holders.key_quarter_holders,
                                                                          second_relay.number);
            bool second_has_first_as_quarter_holder = VectorContainsEntry(second_relay.holders.key_quarter_holders,
                                                                          first_relay.number);
            ASSERT_FALSE(first_has_second_as_quarter_holder and second_has_first_as_quarter_holder);
        }
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
        uint64_t set_of_secrets = KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS;
        uint64_t position = 2;
        complaint.Populate(key_distribution_message.GetHash160(), set_of_secrets, position, *data);
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

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToProcessADurationAfterTheKeyDistributionMessageWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = key_distribution_message.GetHash160();
    EXPECT_THROW(relay_state.ProcessDurationWithoutResponse(duration, *data), RelayStateException);
}


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
    relay_state.ProcessGoodbyeMessage(goodbye);
    uint160 goodbye_message_hash = relay->hashes.goodbye_message_hash;
    ASSERT_THAT(goodbye_message_hash, Eq(goodbye.GetHash160()));
}


class ARelayStateWhichHasProcessedAGoodbyeMessage : public ARelayStateWithAGoodbyeMessage
{
public:
    virtual void SetUp()
    {
        ARelayStateWithAGoodbyeMessage::SetUp();
        relay_state.ProcessGoodbyeMessage(goodbye);
    }

    virtual void TearDown()
    {
        ARelayStateWithAGoodbyeMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage, GeneratesAnObituaryAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(relay, *data, OBITUARY_SAID_GOODBYE));
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage,
       DoesntAssignTheObituaryGeneratedAfterADurationWithoutAResponseAsATaskToKeyQuarterHolders)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_FALSE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage,
       AssignsTheExitingRelaysObituaryAsATaskToTheKeyQuarterHoldersWhenProcessingAGoodbyeComplaint)
{
    GoodbyeComplaint complaint;
    complaint.Populate(goodbye, *data);
    relay_state.ProcessGoodbyeComplaint(complaint, *data);
    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, relay->hashes.obituary_hash));
}


class ARelayStateWhichHasProcessedAnObituary : public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    Obituary obituary;
    uint160 task_hash;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();

        AssignATaskToTheRelay();
        obituary = relay_state.GenerateObituary(relay, OBITUARY_NOT_RESPONDING);
        data->StoreMessage(obituary);
        relay_state.ProcessObituary(obituary);

    }

    void AssignATaskToTheRelay()
    {
        task_hash = 5;
        relay->tasks.push_back(task_hash);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedAnObituary,
       GeneratesARelayExitWhichSpecifiesTheRelaysObituaryAndSecretRecoveryMessages)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    ASSERT_THAT(relay_exit.obituary_hash, Eq(relay->hashes.obituary_hash));
    ASSERT_THAT(relay_exit.secret_recovery_message_hashes, Eq(relay->hashes.secret_recovery_message_hashes));
}

TEST_F(ARelayStateWhichHasProcessedAnObituary, RemovesTheRelayWhenProcessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number;
    relay_state.ProcessRelayExit(relay_exit, *data);
    ASSERT_TRUE(relay_state.GetRelayByNumber(relay_number) == NULL);
}

TEST_F(ARelayStateWhichHasProcessedAnObituary, TransfersTasksFromTheDeadRelayToItsSuccessorWhenProcessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number;
    relay_state.ProcessRelayExit(relay_exit, *data);
    ASSERT_TRUE(VectorContainsEntry(relay_state.GetRelayByNumber(obituary.successor_number)->tasks, task_hash));
}

TEST_F(ARelayStateWhichHasProcessedAnObituary, ThrowsAnExceptionIfTheRelayToRemoveCantBeFound)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    relay_exit.obituary_hash += 1;
    EXPECT_THROW(relay_state.ProcessRelayExit(relay_exit, *data), RelayStateException);
}

std::vector<std::pair<uint64_t, uint64_t> > GetQuarterHolderRoles(uint64_t relay_number, RelayState *state)
{
    std::vector<std::pair<uint64_t, uint64_t> > roles;

    for (uint64_t i = 0; i < state->relays.size(); i++)
        for (uint64_t j = 0; j < state->relays[i].holders.key_quarter_holders.size(); j++)
            if (state->relays[i].holders.key_quarter_holders[j] == relay_number)
                roles.push_back(std::make_pair(state->relays[i].number, j));

    return roles;
};

TEST_F(ARelayStateWhichHasProcessedAnObituary,
       ReplacesTheRelayWithItsSuccessorInAllQuarterHolderRolesWhenProcessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number, successor = obituary.successor_number;

    auto initial_roles_of_exiting_relay = GetQuarterHolderRoles(relay_number, &relay_state);
    ASSERT_THAT(initial_roles_of_exiting_relay.size(), Gt(0));

    relay_state.ProcessRelayExit(relay_exit, *data);

    auto final_roles_of_exiting_relay = GetQuarterHolderRoles(relay_number, &relay_state);
    ASSERT_THAT(final_roles_of_exiting_relay.size(), Eq(0));

    auto final_roles_of_successor = GetQuarterHolderRoles(successor, &relay_state);
    for (auto role : initial_roles_of_exiting_relay)
        ASSERT_TRUE(VectorContainsEntry(final_roles_of_successor, role));
}

TEST_F(ARelayStateWhichHasProcessedAnObituary,
       MarksAKeyQuarterHolderAsNotRespondingAfterADurationWithoutAResponseFromThatRelay)
{
    DurationWithoutResponseFromRelay duration;
    duration.message_hash = obituary.GetHash160();
    duration.relay_number = relay->holders.key_quarter_holders[1];

    relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);

    Relay *key_quarter_holder = relay_state.GetRelayByNumber(duration.relay_number);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(key_quarter_holder, *data, OBITUARY_NOT_RESPONDING));
}


class ARelayStateWithASecretRecoveryMessage : public ARelayStateWhichHasProcessedAnObituary
{
public:
    Relay *key_quarter_holder{NULL};
    SecretRecoveryMessage secret_recovery_message;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAnObituary::SetUp();

        secret_recovery_message = AValidSecretRecoveryMessage();
        data->StoreMessage(secret_recovery_message);
        key_quarter_holder = relay_state.GetRelayByNumber(secret_recovery_message.quarter_holder_number);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAnObituary::TearDown();
    }

    SecretRecoveryMessage AValidSecretRecoveryMessage()
    {
        return ValidSecretRecoveryMessageForKeyQuarterHolder(1);
    }

    SecretRecoveryMessage ValidSecretRecoveryMessageForKeyQuarterHolder(uint32_t quarter_holder_position)
    {
        SecretRecoveryMessage secret_recovery_message;

        secret_recovery_message.obituary_hash = obituary.GetHash160();
        secret_recovery_message.quarter_holder_number = relay->holders.key_quarter_holders[quarter_holder_position];
        secret_recovery_message.successor_number = obituary.successor_number;
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
    ASSERT_THAT(relay->hashes.secret_recovery_message_hashes.size(), Eq(1));
    ASSERT_THAT(relay->hashes.secret_recovery_message_hashes[0], Eq(secret_recovery_message.GetHash160()));
}

TEST_F(ARelayStateWithASecretRecoveryMessage,
       ThrowsAnExceptionIfAnyOfTheNamedRelaysInASecretRecoveryMessageDontExist)
{
    secret_recovery_message.dead_relay_number += 10000;
    EXPECT_THROW(relay_state.ProcessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
    secret_recovery_message.dead_relay_number -= 10000;
    secret_recovery_message.quarter_holder_number += 10000;
    EXPECT_THROW(relay_state.ProcessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
    secret_recovery_message.quarter_holder_number -= 10000;
    secret_recovery_message.successor_number += 10000;
    EXPECT_THROW(relay_state.ProcessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
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
       GeneratesAnObituaryForTheKeyQuarterHolderWhenProcessingASecretRecoveryComplaint)
{
    SecretRecoveryComplaint complaint;
    complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();

    relay_state.ProcessSecretRecoveryComplaint(complaint, *data);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(key_quarter_holder, *data, OBITUARY_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage,
       RemovesTheKeyQuarterHoldersObituaryTaskAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = secret_recovery_message.GetHash160();
    ASSERT_TRUE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    ASSERT_FALSE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
}


class ARelayStateWhichHasProcessedASecretRecoveryComplaint : public ARelayStateWhichHasProcessedASecretRecoveryMessage
{
public:
    SecretRecoveryComplaint complaint;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedASecretRecoveryMessage::SetUp();

        complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();
        complaint.Sign(*data);
        ASSERT_TRUE(complaint.VerifySignature(*data));

        data->StoreMessage(complaint);

        relay_state.ProcessSecretRecoveryComplaint(complaint, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedASecretRecoveryMessage::TearDown();
    }
};


class ARelayStateWithASecretRecoveryFailureMessage : public ARelayStateWithASecretRecoveryMessage
{
public:
    SecretRecoveryFailureMessage failure_message;

    virtual void SetUp()
    {
        ARelayStateWithASecretRecoveryMessage::SetUp();
        auto recovery_messages = AllFourSecretRecoveryMessages();

        vector<uint160> recovery_message_hashes;
        for (auto recovery_message : recovery_messages)
        {
            relay_state.ProcessSecretRecoveryMessage(recovery_message);
            recovery_message_hashes.push_back(recovery_message.GetHash160());
        }

        failure_message.recovery_message_hashes = recovery_message_hashes;
        failure_message.obituary_hash = obituary.GetHash160();
        failure_message.key_sharer_position = 0;
        failure_message.shared_secret_quarter_position = 0;
        failure_message.sum_of_decrypted_shared_secret_quarters = Point(CBigNum(2));
        failure_message.Populate(recovery_message_hashes, *data);
        failure_message.Sign(*data);
        data->StoreMessage(failure_message);
    }

    virtual void TearDown()
    {
        ARelayStateWithASecretRecoveryMessage::TearDown();
    }
};

TEST_F(ARelayStateWithASecretRecoveryFailureMessage, AssignsTheFailureMessageAsATaskToTheKeyQuarterHolders)
{
    relay_state.ProcessSecretRecoveryFailureMessage(failure_message, *data);
    for (auto quarter_holder : relay->QuarterHolders(*data))
        ASSERT_TRUE(VectorContainsEntry(quarter_holder->tasks, failure_message.GetHash160()));
}

TEST_F(ARelayStateWithASecretRecoveryFailureMessage, GeneratesObituariesForRelaysThatDontRespond)
{
    relay_state.ProcessSecretRecoveryFailureMessage(failure_message, *data);
    for (auto quarter_holder : relay->QuarterHolders(*data))
    {
        DurationWithoutResponseFromRelay duration;
        duration.message_hash = failure_message.GetHash160();
        duration.relay_number = quarter_holder->number;
        relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);
        ASSERT_THAT(quarter_holder->hashes.obituary_hash, Ne(0));
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
    auto quarter_holder = relay_state.GetRelayByNumber(relay->holders.key_quarter_holders[0]);
    ASSERT_THAT(quarter_holder->tasks.size(), Eq(2));
    relay_state.ProcessRecoveryFailureAuditMessage(audit_message, *data);
    ASSERT_THAT(quarter_holder->tasks.size(), Eq(1));
}