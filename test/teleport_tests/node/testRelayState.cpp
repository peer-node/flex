#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "RelayState.h"
#include "SecretRecoveryComplaint.h"
#include "SecretRecoveryRefutation.h"

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

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       UnrecordsWhoComplainedAndWhoWasComplainedAboutWhenUnprocessingAKeyDistributionComplaint)
{
    KeyDistributionComplaint complaint(key_distribution_message, KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS, 2);

    relay_state.ProcessKeyDistributionComplaint(complaint, *data);
    relay_state.UnprocessKeyDistributionComplaint(complaint, *data);

    Relay *complainer = complaint.GetComplainer(*data);
    Relay *secret_sender = complaint.GetSecretSender(*data);

    ASSERT_FALSE(VectorContainsEntry(complainer->pending_complaints_sent, complaint.GetHash160()));
    ASSERT_FALSE(VectorContainsEntry(secret_sender->pending_complaints, complaint.GetHash160()));
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
       MarksTheMessageAsUnacceptedWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration_after_key_distribution_message;
    duration_after_key_distribution_message.message_hash = key_distribution_message.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration_after_key_distribution_message, *data);

    relay_state.UnprocessDurationWithoutResponse(duration_after_key_distribution_message, *data);

    ASSERT_THAT(relay->key_distribution_message_accepted, Eq(false));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToProcessAComplaintAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration_after_key_distribution_message;
    duration_after_key_distribution_message.message_hash = key_distribution_message.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration_after_key_distribution_message, *data);

    KeyDistributionComplaint complaint(key_distribution_message, KEY_DISTRIBUTION_COMPLAINT_FIRST_KEY_SIXTEENTHS, 2);
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
    obituary = relay_state.GenerateObituary(relay, OBITUARY_UNREFUTED_COMPLAINT);
    ASSERT_THAT(obituary.in_good_standing, Eq(false));
    obituary = relay_state.GenerateObituary(relay, OBITUARY_REFUTED_COMPLAINT);
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
    ASSERT_THAT(relay->obituary_hash, Eq(obituary.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAKeyDistributionMessage,
       RemovesARelaysObituaryHashWhenUnprocessingTheObituary)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_SAID_GOODBYE);
    relay_state.ProcessObituary(obituary);
    relay_state.UnprocessObituary(obituary);
    ASSERT_THAT(relay->obituary_hash, Eq(0));
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
       RemovesTheObituaryTasksFromKeyQuarterHoldersWhenUnprocessingTheObituary)
{
    Obituary obituary = relay_state.GenerateObituary(relay, OBITUARY_UNREFUTED_COMPLAINT);
    relay_state.ProcessObituary(obituary);
    relay_state.UnprocessObituary(obituary);
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

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       MarksTheComplaintAsNotUpheldWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    relay_state.UnprocessDurationWithoutResponse(duration, *data);

    ASSERT_THAT(secret_sender->upheld_complaints.size(), Eq(0));

    ASSERT_THAT(complainer->pending_complaints_sent.size(), Eq(1));
    ASSERT_THAT(complainer->pending_complaints_sent[0], Eq(complaint.GetHash160()));
    ASSERT_THAT(secret_sender->pending_complaints.size(), Eq(1));
    ASSERT_THAT(secret_sender->pending_complaints[0], Eq(complaint.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       ThrowsAnExceptionIfAskedToProcessADurationAfterTheKeyDistributionMessageWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = key_distribution_message.GetHash160();
    EXPECT_THROW(relay_state.ProcessDurationWithoutResponse(duration, *data), RelayStateException);
}

bool RelayHasAnObituaryWithSpecificReasonForLeaving(Relay *relay, Data data, uint32_t reason)
{
    if (relay->obituary_hash == 0)
        return false;
    std::string message_type = data.msgdata[relay->obituary_hash]["type"];
    if (message_type != "obituary")
        return false;
    Obituary obituary = data.msgdata[relay->obituary_hash]["obituary"];
    return obituary.relay == *relay and obituary.reason_for_leaving == reason;
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       GeneratesAnObituaryForTheSecretSenderAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(secret_sender, *data, OBITUARY_UNREFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       RemovesTheObituaryForTheSecretSenderWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    relay_state.UnprocessDurationWithoutResponse(duration, *data);

    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(secret_sender, *data, OBITUARY_UNREFUTED_COMPLAINT));
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
       MarksTheComplaintAsNotRefutedWhenUnprocessingAKeyDistributionRefutation)
{
    KeyDistributionRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    relay_state.ProcessKeyDistributionRefutation(refutation, *data);

    relay_state.UnprocessKeyDistributionRefutation(refutation, *data);

    ASSERT_THAT(complainer->refutations_of_complaints_sent.size(), Eq(0));

    ASSERT_THAT(complainer->pending_complaints_sent.size(), Eq(1));
    ASSERT_THAT(complainer->pending_complaints_sent[0], Eq(complaint.GetHash160()));
    ASSERT_THAT(secret_sender->pending_complaints.size(), Eq(1));
    ASSERT_THAT(secret_sender->pending_complaints[0], Eq(complaint.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       GeneratesAnObituaryForTheComplainerWhenProcessingAKeyDistributionRefutation)
{
    KeyDistributionRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    relay_state.ProcessKeyDistributionRefutation(refutation, *data);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(complainer, *data, OBITUARY_REFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedAComplaintAboutAKeyDistributionMessage,
       RemovesTheObituaryForTheComplainerWhenUnprocessingAKeyDistributionRefutation)
{
    KeyDistributionRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    relay_state.ProcessKeyDistributionRefutation(refutation, *data);

    relay_state.UnprocessKeyDistributionRefutation(refutation, *data);

    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(complainer, *data, OBITUARY_REFUTED_COMPLAINT));
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

TEST_F(ARelayStateWithAGoodbyeMessage, RemovesTheGoodbyeMessageHashOfTheDeadRelayWhenUnprocessingIt)
{
    relay_state.ProcessGoodbyeMessage(goodbye);
    relay_state.UnprocessGoodbyeMessage(goodbye);
    uint160 goodbye_message_hash = relay->goodbye_message_hash;
    ASSERT_THAT(goodbye_message_hash, Eq(0));
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

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage, RemovesTheObituaryWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    relay_state.UnprocessDurationWithoutResponse(duration, *data);

    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(relay, *data, OBITUARY_SAID_GOODBYE));
}

GoodbyeComplaint AGoodbyeComplaint(GoodbyeMessage goodbye, Data data)
{
    GoodbyeComplaint goodbye_complaint;
    goodbye_complaint.goodbye_message_hash = goodbye.GetHash160();
    goodbye_complaint.key_quarter_holder_position = 1;
    goodbye_complaint.positions_of_bad_encrypted_key_sixteenths.push_back(2);
    goodbye_complaint.Sign(data);

    return goodbye_complaint;
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage, RecordsTheHashOfAGoodbyeComplaint)
{
    auto goodbye_complaint = AGoodbyeComplaint(goodbye, *data);
    ASSERT_TRUE(goodbye_complaint.VerifySignature(*data));
    relay_state.ProcessGoodbyeComplaint(goodbye_complaint, *data);
    ASSERT_TRUE(VectorContainsEntry(relay->pending_complaints, goodbye_complaint.GetHash160()));
    ASSERT_TRUE(VectorContainsEntry(goodbye_complaint.GetComplainer(*data)->pending_complaints_sent,
                                    goodbye_complaint.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeMessage, RemovesTheHashOfAGoodbyeComplaintWhenUnprocessingIt)
{
    auto goodbye_complaint = AGoodbyeComplaint(goodbye, *data);
    relay_state.ProcessGoodbyeComplaint(goodbye_complaint, *data);

    relay_state.UnprocessGoodbyeComplaint(goodbye_complaint, *data);

    ASSERT_FALSE(VectorContainsEntry(relay->pending_complaints, goodbye_complaint.GetHash160()));
    ASSERT_FALSE(VectorContainsEntry(goodbye_complaint.GetComplainer(*data)->pending_complaints_sent,
                                     goodbye_complaint.GetHash160()));
}

class ARelayStateWhichHasProcessedAGoodbyeComplaint : public ARelayStateWhichHasProcessedAGoodbyeMessage
{
public:
    GoodbyeComplaint goodbye_complaint;

    virtual void SetUp()
    {
        ARelayStateWhichHasProcessedAGoodbyeMessage::SetUp();
        goodbye_complaint = AGoodbyeComplaint(goodbye, *data);
        data->StoreMessage(goodbye_complaint);
        relay_state.ProcessGoodbyeComplaint(goodbye_complaint, *data);
    }

    virtual void TearDown()
    {
        ARelayStateWhichHasProcessedAGoodbyeMessage::TearDown();
    }
};

TEST_F(ARelayStateWhichHasProcessedAGoodbyeComplaint, GeneratesAnObituaryForTheDeadRelayAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye_complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(relay, *data, OBITUARY_UNREFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeComplaint,
       RemovesTheObituaryForTheDeadRelayWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = goodbye_complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    relay_state.UnprocessDurationWithoutResponse(duration, *data);

    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(relay, *data, OBITUARY_UNREFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeComplaint,
       GeneratesObituariesForTheDeadRelayAndTheComplainerWhenProcessingAGoodbyeRefutation)
{
    GoodbyeRefutation refutation;
    refutation.complaint_hash = goodbye_complaint.GetHash160();

    relay_state.ProcessGoodbyeRefutation(refutation, *data);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(relay, *data, OBITUARY_SAID_GOODBYE));
    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(goodbye_complaint.GetComplainer(*data), *data,
                                                               OBITUARY_REFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedAGoodbyeComplaint,
       RemovesTheObituariesForTheDeadRelayAndTheComplainerWhenUnprocessingAGoodbyeRefutation)
{
    GoodbyeRefutation refutation;
    refutation.complaint_hash = goodbye_complaint.GetHash160();

    relay_state.ProcessGoodbyeRefutation(refutation, *data);
    relay_state.UnprocessGoodbyeRefutation(refutation, *data);

    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(relay, *data, OBITUARY_SAID_GOODBYE));
    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(goodbye_complaint.GetComplainer(*data), *data,
                                                               OBITUARY_REFUTED_COMPLAINT));
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

TEST_F(ARelayStateWhichHasProcessedAnObituary, ReaddsTheRelayWhenUnprocessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number;
    relay_state.ProcessRelayExit(relay_exit, *data);

    relay_state.UnprocessRelayExit(relay_exit, *data);
    ASSERT_FALSE(relay_state.GetRelayByNumber(relay_number) == NULL);
}

TEST_F(ARelayStateWhichHasProcessedAnObituary, TransfersTasksFromTheDeadRelayToItsSuccessorWhenProcessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number;
    relay_state.ProcessRelayExit(relay_exit, *data);
    ASSERT_TRUE(VectorContainsEntry(relay_state.GetRelayByNumber(obituary.successor_number)->tasks, task_hash));
}

TEST_F(ARelayStateWhichHasProcessedAnObituary, TransfersTheDeadRelaysTasksBackToItWhenUnprocessingARelayExit)
{
    RelayExit relay_exit = relay_state.GenerateRelayExit(relay);
    uint64_t relay_number = relay->number;
    relay_state.ProcessRelayExit(relay_exit, *data);
    relay_state.UnprocessRelayExit(relay_exit, *data);

    ASSERT_TRUE(VectorContainsEntry(relay_state.GetRelayByNumber(obituary.relay.number)->tasks, task_hash));
    ASSERT_FALSE(VectorContainsEntry(relay_state.GetRelayByNumber(obituary.successor_number)->tasks, task_hash));
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
    duration.relay_number = relay->key_quarter_holders[1];

    relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);

    Relay *key_quarter_holder = relay_state.GetRelayByNumber(duration.relay_number);

    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(key_quarter_holder, *data, OBITUARY_NOT_RESPONDING));
}

TEST_F(ARelayStateWhichHasProcessedAnObituary,
       RemovesTheNotRespondingObituaryFromTheKeyQuarterHolderWhenUnprocessingADurationWithoutAResponseFromThatRelay)
{
    DurationWithoutResponseFromRelay duration;
    duration.message_hash = obituary.GetHash160();
    duration.relay_number = relay->key_quarter_holders[1];

    relay_state.ProcessDurationWithoutResponseFromRelay(duration, *data);
    relay_state.UnprocessDurationWithoutResponseFromRelay(duration, *data);

    Relay *key_quarter_holder = relay_state.GetRelayByNumber(duration.relay_number);
    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(key_quarter_holder, *data, OBITUARY_NOT_RESPONDING));
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
        SecretRecoveryMessage secret_recovery_message;

        secret_recovery_message.obituary_hash = obituary.GetHash160();
        secret_recovery_message.quarter_holder_number = relay->key_quarter_holders[1];
        secret_recovery_message.successor_number = obituary.successor_number;
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

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage, RecordsTheHashOfASecretRecoveryComplaint)
{
    SecretRecoveryComplaint complaint;
    complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();

    relay_state.ProcessSecretRecoveryComplaint(complaint, *data);

    ASSERT_TRUE(VectorContainsEntry(key_quarter_holder->pending_complaints, complaint.GetHash160()));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage, RemovesTheHashOfASecretRecoveryComplaintWhenUnprocessingIt)
{
    SecretRecoveryComplaint complaint;
    complaint.secret_recovery_message_hash = secret_recovery_message.GetHash160();
    relay_state.ProcessSecretRecoveryComplaint(complaint, *data);

    relay_state.UnprocessSecretRecoveryComplaint(complaint, *data);
    ASSERT_FALSE(VectorContainsEntry(key_quarter_holder->pending_complaints, complaint.GetHash160()));
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

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryMessage,
       ReaddsTheKeyQuarterHoldersObituaryTaskWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = secret_recovery_message.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    relay_state.UnprocessDurationWithoutResponse(duration, *data);
    ASSERT_TRUE(VectorContainsEntry(key_quarter_holder->tasks, obituary.GetHash160()));
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

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint,
       GeneratesAnObituaryForTheKeyQuarterHolderAfterADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();

    relay_state.ProcessDurationWithoutResponse(duration, *data);
    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(key_quarter_holder, *data, OBITUARY_UNREFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint,
       RemovesTheObituaryFromTheKeyQuarterHolderWhenUnprocessingADurationWithoutAResponse)
{
    DurationWithoutResponse duration;
    duration.message_hash = complaint.GetHash160();
    relay_state.ProcessDurationWithoutResponse(duration, *data);

    relay_state.UnprocessDurationWithoutResponse(duration, *data);
    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(key_quarter_holder, *data, OBITUARY_UNREFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint,
       GeneratesAnObituaryForTheComplainerWhenProcessingASecretRecoveryRefutation)
{
    SecretRecoveryRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    refutation.Sign(*data);
    ASSERT_TRUE(refutation.VerifySignature(*data));

    relay_state.ProcessSecretRecoveryRefutation(refutation, *data);
    ASSERT_TRUE(RelayHasAnObituaryWithSpecificReasonForLeaving(complaint.GetComplainer(*data), *data,
                                                               OBITUARY_REFUTED_COMPLAINT));
}

TEST_F(ARelayStateWhichHasProcessedASecretRecoveryComplaint,
       RemovesTheObituaryFromTheComplainerWhenUnprocessingASecretRecoveryRefutation)
{
    SecretRecoveryRefutation refutation;
    refutation.complaint_hash = complaint.GetHash160();
    relay_state.ProcessSecretRecoveryRefutation(refutation, *data);

    relay_state.UnprocessSecretRecoveryRefutation(refutation, *data);
    ASSERT_FALSE(RelayHasAnObituaryWithSpecificReasonForLeaving(complaint.GetComplainer(*data), *data,
                                                                OBITUARY_REFUTED_COMPLAINT));
}
