#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "RelayState.h"


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
    uint160 join_message_hash = relay_state.relays[0].join_message_hash;
    ASSERT_THAT(join_message_hash, Eq(relay_join_message.GetHash160()));
}

TEST_F(ARelayState, ThrowsAnExceptionIfTheSameMinedCreditMessageHashIsUsedTwiceInJoinMessages)
{
    RelayJoinMessage relay_join_message;
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    EXPECT_THROW(relay_state.ProcessRelayJoinMessage(relay_join_message), RelayStateException);
}

TEST_F(ARelayState, RemovesTheLastRelayAddedWhenUnprocessingItsRelayJoinMessage)
{
    RelayJoinMessage relay_join_message = Relay().GenerateJoinMessage(keydata, 1);
    relay_state.ProcessRelayJoinMessage(relay_join_message);
    relay_state.UnprocessRelayJoinMessage(relay_join_message);
    ASSERT_THAT(relay_state.relays.size(), Eq(0));
}

TEST_F(ARelayState, ThrowsAnExceptionIfAskedToUnprocessARelayJoinMessageWhichWasntTheMostRecent)
{
    RelayJoinMessage first_relay_join_message = Relay().GenerateJoinMessage(keydata, 1);
    relay_state.ProcessRelayJoinMessage(first_relay_join_message);
    RelayJoinMessage second_relay_join_message = Relay().GenerateJoinMessage(keydata, 2);
    relay_state.ProcessRelayJoinMessage(second_relay_join_message);
    EXPECT_THROW(relay_state.UnprocessRelayJoinMessage(first_relay_join_message), RelayStateException);
}


class ARelayStateWith37Relays : public ARelayState
{
public:
    virtual void SetUp()
    {
        ARelayState::SetUp();
        uint64_t start = GetTimeMicros();
        static RelayState reference_test_relay_state;
        if (reference_test_relay_state.relays.size() == 0)
        {
            for (uint32_t i = 1; i <= 37; i++)
            {
                RelayJoinMessage join_message = Relay().GenerateJoinMessage(keydata, i);
                relay_state.ProcessRelayJoinMessage(join_message);
            }
            reference_test_relay_state = relay_state;
        }
        else
            relay_state = reference_test_relay_state;
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
            ASSERT_THAT(relay.key_quarter_holders.size(), Eq(4));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.key_quarter_holders, relay.number));
        }
    }
}

TEST_F(ARelayStateWith37Relays, AssignsKeySixteenthHoldersAtLeastOneOfWhichJoinedLaterThanTheGivenRelay)
{
    for (auto relay : relay_state.relays)
    {
        if (relay_state.AssignKeyPartHoldersToRelay(relay, 1))
        {
            ASSERT_THAT(relay.first_set_of_key_sixteenth_holders.size(), Eq(16));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.first_set_of_key_sixteenth_holders, relay.number));
            ASSERT_THAT(relay.second_set_of_key_sixteenth_holders.size(), Eq(16));
            ASSERT_TRUE(VectorContainsANumberGreaterThan(relay.second_set_of_key_sixteenth_holders, relay.number));
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
            bool first_has_second_as_quarter_holder = VectorContainsEntry(first_relay.key_quarter_holders,
                                                                          second_relay.number);
            bool second_has_first_as_quarter_holder = VectorContainsEntry(second_relay.key_quarter_holders,
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
        return relay_state.relays[0].key_quarter_holders[1];
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
    ASSERT_THAT(relay->key_distribution_message_hash, Eq(key_distribution_message.GetHash160()));
}

TEST_F(ARelayStateWith37RelaysAndAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToProcessAKeyDistributionMessageFromARelayWhichHasAlreadySentOne)
{
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    EXPECT_THROW(relay_state.ProcessKeyDistributionMessage(key_distribution_message), RelayStateException);
}

TEST_F(ARelayStateWith37RelaysAndAKeyDistributionMessage, UnprocessesAKeyDistributionMessage)
{
    relay_state.ProcessKeyDistributionMessage(key_distribution_message);
    relay_state.UnprocessKeyDistributionMessage(key_distribution_message);
    ASSERT_THAT(relay->key_distribution_message_hash, Eq(0));
}

TEST_F(ARelayStateWith37RelaysAndAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToUnprocessAKeyDistributionMessageWhichHasNotBeenProcessed)
{
    EXPECT_THROW(relay_state.UnprocessKeyDistributionMessage(key_distribution_message), RelayStateException);
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

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       RecordsWhoComplainedAndWhoWasComplainedAboutWhenProcessingAKeyDistributionComplaint)
{
    uint64_t set_of_secrets = KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS;
    uint64_t position = 2;
    KeyDistributionComplaint complaint(key_distribution_message, set_of_secrets, position);

    relay_state.ProcessKeyDistributionComplaint(complaint, *data);

    Relay *complainer = complaint.GetComplainer(*data);
    Relay *secret_sender = complaint.GetSecretSender(*data);

    ASSERT_TRUE(VectorContainsEntry(complainer->pending_complaints_sent, complaint.GetHash160()));
    ASSERT_TRUE(VectorContainsEntry(secret_sender->pending_complaints, complaint.GetHash160()));
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
    obituary = relay_state.GenerateObituary(relay, OBITUARY_UNREFUTED_COMPLAINT);
    ASSERT_THAT(obituary.in_good_standing, Eq(false));
    obituary = relay_state.GenerateObituary(relay, OBITUARY_REFUTED_COMPLAINT);
    ASSERT_THAT(obituary.in_good_standing, Eq(false));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       GeneratesAnObituaryWhichSpecifiesTheDeadRelaysSuccessor)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    ASSERT_THAT(obituary.successor, Eq(relay_state.AssignSuccessorToRelay(relay)));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       RecordsARelaysObituaryHashWhenProcessingTheObituary)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    relay_state.ProcessObituary(obituary);
    ASSERT_THAT(relay->obituary_hash, Eq(obituary.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsTheObituaryAsATaskForTheKeyQuarterHoldersWhenProcessingItIfTheRelayDidntSayGoodbyeCorrectly)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_UNREFUTED_COMPLAINT);
    relay_state.ProcessObituary(obituary);
    for (auto key_quarter_holder_relay_number : relay->key_quarter_holders)
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
    for (auto key_quarter_holder_relay_number : relay->key_quarter_holders)
    {
        auto key_quarter_holder = relay_state.GetRelayByNumber(key_quarter_holder_relay_number);
        ASSERT_FALSE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
    }
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsADistinctSuccessorWhichIsNotAKeyQuarterHolderToARelayWhichIsExiting)
{
    uint64_t successor_relay_number = relay_state.AssignSuccessorToRelay(relay);
    ASSERT_FALSE(VectorContainsEntry(relay->key_quarter_holders, successor_relay_number));
    ASSERT_THAT(relay->number, Ne(successor_relay_number));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       AssignsASuccessorSuchThatNoTwoRelaysAreEachOthersKeyQuarterHolders)
{
    uint64_t successor_relay_number = relay_state.AssignSuccessorToRelay(relay);

    for (Relay &key_sharer : relay_state.relays)
        if (VectorContainsEntry(key_sharer.key_quarter_holders, relay->number))
        {
            EraseEntryFromVector(relay->number, key_sharer.key_quarter_holders);
            key_sharer.key_quarter_holders.push_back(successor_relay_number);
        }

    for (Relay &first_relay : relay_state.relays)
        for (Relay &second_relay : relay_state.relays)
        {
            bool first_has_second_as_quarter_holder = VectorContainsEntry(first_relay.key_quarter_holders,
                                                                          second_relay.number);
            bool second_has_first_as_quarter_holder = VectorContainsEntry(second_relay.key_quarter_holders,
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

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();
        uint64_t set_of_secrets = KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS;
        uint64_t position = 2;
        complaint = KeyDistributionComplaint(key_distribution_message, set_of_secrets, position);
        data->StoreMessage(complaint);
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
       MarksTheComplaintAsUpheldAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    ASSERT_THAT(secret_sender->upheld_complaints.size(), Eq(1));
    ASSERT_THAT(secret_sender->upheld_complaints[0], Eq(complaint.GetHash160()));

    ASSERT_THAT(complainer->pending_complaints_sent.size(), Eq(0));
    ASSERT_THAT(secret_sender->pending_complaints.size(), Eq(0));
}

bool RelayHasAnObituary(Relay *relay, Data data)
{
    if (relay->obituary_hash == 0)
        return false;
    std::string message_type = data.msgdata[relay->obituary_hash]["type"];
    if (message_type != "obituary")
        return false;
    Obituary obituary = data.msgdata[relay->obituary_hash]["obituary"];
    return obituary.relay == *relay;
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       GeneratesAnObituaryForTheSecretSenderAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    ASSERT_TRUE(RelayHasAnObituary(secret_sender, *data));
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       MarksTheComplaintAsRefutedWhenProcessingAKeyDistributionRefutation)
{
    KeyDistributionRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    relay_state.ProcessKeyDistributionRefutation(refutation, *data);

    ASSERT_THAT(complainer->refutations_of_complaints_sent.size(), Eq(1));
    ASSERT_THAT(complainer->refutations_of_complaints_sent[0], Eq(refutation.GetHash160()));

    ASSERT_THAT(complainer->pending_complaints_sent.size(), Eq(0));
    ASSERT_THAT(secret_sender->pending_complaints.size(), Eq(0));
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       GeneratesAnObituaryForTheComplainerWhenProcessingAKeyDistributionRefutation)
{
    KeyDistributionRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    relay_state.ProcessKeyDistributionRefutation(refutation, *data);

    ASSERT_TRUE(RelayHasAnObituary(complainer, *data));
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
    uint160 goodbye_message_hash = relay->goodbye_message_hash;
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

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage, GeneratesAnObituaryInGoodStandingAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);
    ASSERT_TRUE(RelayHasAnObituary(relay, *data));
}

class ARelayStateWhichHasProcessedAnObituary : public ARelayStateWhichHasProcessedAKeyDistributionMessage
{
public:
    Obituary obituary;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAKeyDistributionMessage::SetUp();
        obituary = relay_state.GenerateObituary(relay, OBITUARY_NOT_RESPONDING);
        data->StoreMessage(obituary);
        relay_state.ProcessObituary(obituary);
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
    ASSERT_THAT(relay_exit.obituary_hash, Eq(relay->obituary_hash));
    ASSERT_THAT(relay_exit.secret_recovery_message_hashes, Eq(relay->secret_recovery_message_hashes));
}

TEST_F(ARelayStateWhichHasProcessedAnObituary, RemovesTheRelayWhenProcessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number;
    relay_state.ProcessRelayExit(relay_exit, *data);
    ASSERT_TRUE(relay_state.GetRelayByNumber(relay_number) == NULL);
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
        for (uint64_t j = 0; j < state->relays[i].key_quarter_holders.size(); j++)
            if (state->relays[i].key_quarter_holders[j] == relay_number)
                roles.push_back(std::make_pair(state->relays[i].number, j));

    return roles;
};

TEST_F(ARelayStateWhichHasProcessedAnObituary,
       RelacesTheRelayWithItsSuccessorInAllQuarterHolderRolesWhenProcessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number, successor = obituary.successor;

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
    duration.relay_number = relay->key_quarter_holders[1];

    relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);

    Relay *key_quarter_holder = relay_state.GetRelayByNumber(duration.relay_number);

    ASSERT_TRUE(RelayHasAnObituary(key_quarter_holder, *data));
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
        key_quarter_holder = relay_state.GetRelayByNumber(secret_recovery_message.quarter_holder_number);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAnObituary::TearDown();
    }

    SecretRecoveryMessage AValidSecretRecoveryMessage()
    {
        SecretRecoveryMessage secret_recovery_message;

        secret_recovery_message.obituary_hash = obituary.GetHash160();
        secret_recovery_message.quarter_holder_number = relay->key_quarter_holders[1];
        secret_recovery_message.successor_number = obituary.successor;
        secret_recovery_message.dead_relay_number = relay->number;

        secret_recovery_message.Sign(*data);
        return secret_recovery_message;
    }
};

TEST_F(ARelayStateWithASecretRecoveryMessage, RecordsTheHashWhenProcessingTheSecretRecoveryMessage)
{
    relay_state.ProcessSecretRecoveryMessage(secret_recovery_message);
    ASSERT_THAT(relay->secret_recovery_message_hashes.size(), Eq(1));
    ASSERT_THAT(relay->secret_recovery_message_hashes[0], Eq(secret_recovery_message.GetHash160()));
}

TEST_F(ARelayStateWithASecretRecoveryMessage,
       RemovesTheKeyQuarterHoldersObituaryTaskWhenProcessingSecretRecoveryMessage)
{
    ASSERT_TRUE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));

    relay_state.ProcessSecretRecoveryMessage(secret_recovery_message);

    ASSERT_FALSE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
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

TEST_F(ARelayStateWithASecretRecoveryMessage,
       ThrowsAnExceptionIfAskedToUnprocessASecretRecoveryMessageWhichNamesNonExistantRelays)
{
    secret_recovery_message.dead_relay_number += 10000;
    EXPECT_THROW(relay_state.UnprocessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
    secret_recovery_message.dead_relay_number -= 10000;
    secret_recovery_message.quarter_holder_number += 10000;
    EXPECT_THROW(relay_state.UnprocessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
    secret_recovery_message.quarter_holder_number -= 10000;
    secret_recovery_message.successor_number += 10000;
    EXPECT_THROW(relay_state.UnprocessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
}

TEST_F(ARelayStateWithASecretRecoveryMessage,
       ThrowsAnExceptionIfAskedToUnprocessASecretRecoveryMessageWhichHasntBeenProcessed)
{
    EXPECT_THROW(relay_state.UnprocessSecretRecoveryMessage(secret_recovery_message), RelayStateException);
}

TEST_F(ARelayStateWithASecretRecoveryMessage,
       RemovesTheSecretRecoveryMessagesHashWhenUnprocessingIt)
{
    relay_state.ProcessSecretRecoveryMessage(secret_recovery_message);
    relay_state.UnprocessSecretRecoveryMessage(secret_recovery_message);
    ASSERT_THAT(relay->secret_recovery_message_hashes.size(), Eq(0));
}

