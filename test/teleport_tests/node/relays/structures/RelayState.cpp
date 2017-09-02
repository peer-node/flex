#include <src/vector_tools.h>
#include <src/credits/creditsign.h>
#include <boost/range/adaptor/reversed.hpp>
#include "RelayState.h"
#include "test/teleport_tests/node/relays/messages/SuccessionCompletedAuditComplaint.h"

using std::vector;

#include "log.h"
#define LOG_CATEGORY "RelayState.cpp"


void RelayState::ProcessRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    ThrowExceptionIfMinedCreditMessageHashWasAlreadyUsed(relay_join_message);
    Relay relay = GenerateRelayFromRelayJoinMessage(relay_join_message);
    relays.push_back(relay);
    AssignNewKeyQuarterHoldersAndSuccessorsAfterNewRelayHasJoined(&relay);

    maps_are_up_to_date = false;
}

void RelayState::AssignNewKeyQuarterHoldersAndSuccessorsAfterNewRelayHasJoined(Relay *new_relay)
{
    if (relays.back().number < 53)
        return;
    for (auto &relay : relays)
    {
        if (not relay.HasFourKeyQuarterHolders() and RelayIsASuitableQuarterHolder(new_relay, (int64_t)relay.number))
            AssignKeyQuarterHoldersToRelay(relay);
        if (relay.current_successor_number == 0 and SuccessorToRelayIsSuitable(new_relay, &relay))
            AssignSuccessorToRelay(&relay);
    }
}

void RelayState::ThrowExceptionIfMinedCreditMessageHashWasAlreadyUsed(RelayJoinMessage relay_join_message)
{
    if (MinedCreditMessageHashIsAlreadyBeingUsed(relay_join_message.mined_credit_message_hash))
        throw RelayStateException("mined_credit_message_hash already used");
}

bool RelayState::MinedCreditMessageHashIsAlreadyBeingUsed(uint160 mined_credit_message_hash)
{
    for (Relay& relay : relays)
        if (relay.hashes.mined_credit_message_hash == mined_credit_message_hash)
            return true;
    return false;
}

Relay RelayState::GenerateRelayFromRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    Relay new_relay;
    if (relays.size() == 0)
        new_relay.number = 1;
    else
        new_relay.number = relays.back().number + 1;

    new_relay.hashes.join_message_hash = relay_join_message.GetHash160();
    new_relay.hashes.mined_credit_message_hash = relay_join_message.mined_credit_message_hash;
    new_relay.public_signing_key = relay_join_message.PublicSigningKey();
    new_relay.public_key_set = relay_join_message.public_key_set;
    return new_relay;
}

void RelayState::AssignRemainingKeyQuarterHoldersToRelay(Relay &relay)
{
    for (uint32_t quarter_holder_position = 1; quarter_holder_position < 4; quarter_holder_position++)
        AssignKeyQuarterHolderToRelay(relay, quarter_holder_position);
}

bool RelayState::AssignKeyQuarterHolderToRelay(Relay &relay, uint32_t quarter_holder_position)
{
    for (int64_t n = (int64_t)relay.number - 11; n >= 0 and n >= relays.front().number; n--)
        if (RelayIsASuitableQuarterHolder(&relay, n))
        {
            relay.key_quarter_holders[quarter_holder_position] = (uint64_t)n;
            return true;
        }
    for (int64_t n = (int64_t)relay.number + 1; n <= relays.back().number; n++)
        if (RelayIsASuitableQuarterHolder(&relay, n))
        {
            relay.key_quarter_holders[quarter_holder_position] = (uint64_t)n;
            return true;
        }
    return false;
}

bool RelayState::RelayIsASuitableQuarterHolder(Relay *key_sharer, int64_t candidate_relay_number)
{
    auto &candidate = relays[candidate_relay_number];

    if (candidate.status != ALIVE or key_sharer->number == candidate_relay_number)
        return false;
    if (ArrayContainsEntry(key_sharer->key_quarter_holders, (uint64_t) candidate_relay_number))
        return false;;
    return true;
}
uint64_t RelayState::KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed)
{
    uint64_t chosen_relay_number = SelectRandomKeyQuarterHolderWhichIsntAlreadyBeingUsed(relay, random_seed);
    while (chosen_relay_number <= relay.number)
    {
        random_seed = HashUint160(random_seed);
        chosen_relay_number = SelectRandomKeyQuarterHolderWhichIsntAlreadyBeingUsed(relay, random_seed);
    }
    return chosen_relay_number;
}

uint64_t RelayState::SelectRandomKeyQuarterHolderWhichIsntAlreadyBeingUsed(Relay &relay, uint160 relay_chooser)
{
    bool found = false;
    uint64_t relay_position = 0;
    while (not found)
    {
        relay_chooser = HashUint160(relay_chooser);
        relay_position = (CBigNum(relay_chooser) % CBigNum(relays.size())).getulong();
        auto chosen_relay = relays[relay_position];
        found = chosen_relay.number != relay.number and chosen_relay.status == ALIVE and
                    not ArrayContainsEntry(relay.key_quarter_holders, chosen_relay.number);
    }
    return relays[relay_position].number;
}

Relay *RelayState::GetRelayByNumber(uint64_t number)
{
    EnsureMapsAreUpToDate();
    return relays_by_number.count(number) ? relays_by_number[number] : NULL;
}

Relay *RelayState::GetRelayByJoinHash(uint160 join_message_hash)
{
    EnsureMapsAreUpToDate();
    return relays_by_join_hash.count(join_message_hash) ? relays_by_join_hash[join_message_hash] : NULL;
}

bool RelayState::ThereAreEnoughRelaysToAssignKeyQuarterHolders(Relay &relay)
{
    uint64_t number_of_relays_that_joined_later = NumberOfEligibleRelaysThatJoinedLaterThan(relay);
    uint64_t number_of_relays_that_joined_before = NumberOfEligibleRelaysThatJoinedBefore(relay);

    return number_of_relays_that_joined_later >= 1 and
            number_of_relays_that_joined_before + number_of_relays_that_joined_later >= 4;
}

bool RelayState::AssignKeyQuarterHoldersToRelay(Relay &relay)
{
    if (not ThereAreEnoughRelaysToAssignKeyQuarterHolders(relay))
        return false;
    AssignKeyQuarterHolderWhoJoinedLater(relay);
    AssignRemainingKeyQuarterHoldersToRelay(relay);
    return true;
}

void RelayState::AssignKeyQuarterHolderWhoJoinedLater(Relay &relay)
{
    for (auto &candidate : relays)
    {
        if (candidate.number <= relay.number)
            continue;

        if (candidate.status == ALIVE)
        {
            relay.key_quarter_holders[0] = candidate.number;
            return;
        }
    }
}

uint64_t RelayState::NumberOfEligibleRelaysThatJoinedLaterThan(Relay &given_relay)
{
    uint64_t count = 0;
    for (auto &relay : relays)
        if (relay.status == ALIVE and relay.number > given_relay.number)
            count++;
    return count;
}

uint64_t RelayState::NumberOfEligibleRelaysThatJoinedBefore(Relay &relay)
{
    uint64_t count = 0;
    for (uint64_t n = 0; n < relays.size() and relays[n].number < relay.number; n++)
        if (relays[n].status == ALIVE)
            count++;
    return count;
}

std::vector<uint64_t> RelayState::GetKeyQuarterHoldersAsListOf24RelayNumbers(uint64_t relay_number)
{
    Relay *relay = GetRelayByNumber(relay_number);
    if (relay == NULL) throw RelayStateException("no such relay");
    std::vector<uint64_t> recipient_relay_numbers;
    for (auto quarter_key_holder : relay->key_quarter_holders)
    {
        for (uint64_t i = 0; i < 6; i++)
            recipient_relay_numbers.push_back(quarter_key_holder);
    }
    return recipient_relay_numbers;
}

void RelayState::ProcessKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    auto relay = GetRelayByJoinHash(key_distribution_message.relay_join_hash);

    RecordKeyDistributionMessage(relay, key_distribution_message.GetHash160());
    RemoveKeyDistributionTask(relay, key_distribution_message);
}

void RelayState::RecordKeyDistributionMessage(Relay *relay, uint160 key_distribution_message_hash)
{
    for (uint32_t position = 0; position < 4; position++)
    {
        relay->key_quarter_locations[position].message_hash = key_distribution_message_hash;
        relay->key_quarter_locations[position].position = position;
    }
}

void RelayState::RemoveKeyDistributionTask(Relay *relay, KeyDistributionMessage &key_distribution_message)
{
    Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay->hashes.join_message_hash, key_distribution_message.position);
    EraseEntryFromVector(task, relay->tasks);
}

void RelayState::EnsureMapsAreUpToDate()
{
    if (maps_are_up_to_date)
        return;
    relays_by_number.clear();
    relays_by_join_hash.clear();
    for (Relay &relay : relays)
    {
        relays_by_number[relay.number] = &relay;
        relays_by_join_hash[relay.hashes.join_message_hash] = &relay;
    }
    maps_are_up_to_date = true;
}

uint64_t RelayState::AssignSuccessorToRelay(Relay *relay)
{
    relay->current_successor_number = GetEligibleSuccessorForRelay(relay);
    return relay->current_successor_number;
}

uint64_t RelayState::GetEligibleSuccessorForRelay(Relay *relay)
{
    Relay *chosen_successor{NULL};

    for (uint64_t successor_number = relay->number + 1; successor_number < relays.back().number; successor_number++)
    {
        chosen_successor = GetRelayByNumber(successor_number);
        if (SuccessorToRelayIsSuitable(chosen_successor, relay))
            return chosen_successor->number;
    }
    for (uint64_t successor_number = relay->number - 1; successor_number > relays.front().number; successor_number--)
    {
        chosen_successor = GetRelayByNumber(successor_number);
        if (SuccessorToRelayIsSuitable(chosen_successor, relay))
            return chosen_successor->number;
    }
    return 0;
}

bool RelayState::SuccessorToRelayIsSuitable(Relay *chosen_successor, Relay *relay)
{
    if (chosen_successor->status != ALIVE)
        return false;
    if (chosen_successor->number == relay->number)
        return false;
    if (ArrayContainsEntry(relay->key_quarter_holders, chosen_successor->number))
        return false;
    vector<uint64_t> key_quarter_sharers_of_relay = RelayNumbersOfRelaysWhoseKeyQuartersAreHeldByGivenRelay(relay);
    if (VectorContainsEntry(key_quarter_sharers_of_relay, chosen_successor->number))
        return false;
    for (auto key_quarter_sharer_number : key_quarter_sharers_of_relay)
        if (ArrayContainsEntry(chosen_successor->key_quarter_holders, key_quarter_sharer_number))
            return false;
    return true;
}

std::vector<uint64_t> RelayState::RelayNumbersOfRelaysWhoseKeyQuartersAreHeldByGivenRelay(Relay *given_relay)
{
    std::vector<uint64_t> relays_with_key_quarters_held;

    for (Relay &relay : relays)
        if (ArrayContainsEntry(relay.key_quarter_holders, given_relay->number))
            relays_with_key_quarters_held.push_back(relay.number);

    return relays_with_key_quarters_held;
}

void RelayState::AssignSecretRecoveryTasksToKeyQuarterHolders(Relay *dead_relay)
{
    for (uint32_t position = 0; position < 4; position++)
        AssignSecretRecoveryTaskToKeyQuarterHolder(dead_relay, position);
}

void RelayState::AssignSecretRecoveryTaskToKeyQuarterHolder(Relay *dead_relay, uint32_t position)
{
    auto key_quarter_holder = GetRelayByNumber(dead_relay->key_quarter_holders[position]);
    if (key_quarter_holder == NULL)
    {
        log_ << "relay " << dead_relay->number << " has no quarter holder for position " << position << "\n";
        return;
    }
    Task task(SEND_SECRET_RECOVERY_MESSAGE, Death(dead_relay->number), position);
    key_quarter_holder->tasks.push_back(task);
}

void RelayState::TransferInheritableTasksToSuccessor(uint64_t dead_relay_number, uint64_t successor_number)
{
    Relay *successor = GetRelayByNumber(successor_number);
    Relay *dead_relay = GetRelayByNumber(dead_relay_number);
    if (dead_relay == NULL or successor == NULL) throw RelayStateException("no such relay");

    for (auto task : dead_relay->tasks)
        if (task.IsInheritable())
            successor->tasks.push_back(task);
}

void RelayState::ReplaceDeadRelayWithSuccessorInQuarterHolderRoles(uint64_t dead_relay_number,
                                                                   uint64_t successor_relay_number)
{
    for (uint32_t i = 0; i < relays.size(); i++)
    {
        for (uint32_t j = 0; j < relays[i].key_quarter_holders.size(); j++)
            if (relays[i].key_quarter_holders[j] == dead_relay_number)
                relays[i].key_quarter_holders[j] = successor_relay_number;
    }
}

void RelayState::ProcessKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data)
{
    auto key_sharer = complaint.GetSecretSender(data);
    if (key_sharer == NULL) throw RelayStateException("no such relay");

    key_sharer->hashes.key_distribution_complaint_hashes.push_back(complaint.GetHash160());

    RecordRelayDeath(key_sharer, data, MISBEHAVED);
    RecordSentComplaint(complaint.GetComplainer(data));
}

void RelayState::RecordSentComplaint(Relay *complainer)
{
    if (complainer == NULL) throw RelayStateException("RecordSentComplaint: no such relay");
    complainer->number_of_complaints_sent += 1;
}

void RelayState::ProcessDurationWithoutResponse(DurationWithoutResponse duration, Data data)
{
    std::string message_type = data.msgdata[duration.message_hash]["type"];
    uint160 hash = duration.message_hash;

    if (message_type == "key_distribution_audit")
        ProcessDurationWithoutResponseAfterKeyDistributionAuditMessage(data.msgdata[hash][message_type], data);

    else if (message_type == "four_secret_recovery_messages")
        ProcessDurationWithoutResponseAfterFourSecretRecoveryMessages(data.msgdata[hash][message_type], data);

    else if (message_type == "succession_completed_audit")
        ProcessDurationWithoutResponseAfterSuccessionCompletedAuditMessage(data.msgdata[hash][message_type], data);
}

void RelayState::ProcessDurationWithoutResponseAfterKeyDistributionAuditMessage(
        KeyDistributionAuditMessage audit_message, Data data)
{
    Relay *relay = GetRelayByNumber(audit_message.relay_number);
    relay->key_distribution_message_audited = true;
}

void RelayState::ProcessDurationWithoutResponseAfterFourSecretRecoveryMessages(
        FourSecretRecoveryMessages four_messages, Data data)
{
    SecretRecoveryMessage secret_recovery_message = data.GetMessage(four_messages.secret_recovery_messages[0]);
    Relay *successor = secret_recovery_message.GetSuccessor(data);
    RecordRelayDeath(successor, data, NOT_RESPONDING);
}

void RelayState::ProcessDurationWithoutResponseAfterSuccessionCompletedAuditMessage(
        SuccessionCompletedAuditMessage audit_message, Data data)
{
    uint160 succession_completed_hash = audit_message.succession_completed_message_hash;
    SuccessionCompletedMessage succession_completed = data.GetMessage(succession_completed_hash);
    CopyCurrentKeyQuarterLocationsToListOfPreviousLocations(succession_completed);
    RecordSuccessionCompletedMessageAsTheLocationForRecoveredKeyQuarters(succession_completed_hash,
                                                                         succession_completed);
}

void RelayState::CopyCurrentKeyQuarterLocationsToListOfPreviousLocations(
        SuccessionCompletedMessage &succession_completed_message)
{
    for (uint32_t i = 0; i < succession_completed_message.key_quarter_sharers.size(); i++)
    {
        auto key_sharer = GetRelayByNumber(succession_completed_message.key_quarter_sharers[i]);
        auto &location = key_sharer->key_quarter_locations[succession_completed_message.key_quarter_positions[i]];
        if (not VectorContainsEntry(key_sharer->previous_key_quarter_locations, location.message_hash))
            key_sharer->previous_key_quarter_locations.push_back(location.message_hash);
    }
}

void RelayState::RecordSuccessionCompletedMessageAsTheLocationForRecoveredKeyQuarters(
        uint160 succession_completed_message_hash, SuccessionCompletedMessage &succession_completed_message)
{
    for (uint32_t i = 0; i < succession_completed_message.key_quarter_sharers.size(); i++)
    {
        auto key_sharer = GetRelayByNumber(succession_completed_message.key_quarter_sharers[i]);
        auto &location = key_sharer->key_quarter_locations[succession_completed_message.key_quarter_positions[i]];
        location.message_hash = succession_completed_message_hash;
        location.position = i;
    }
}


void RelayState::ProcessGoodbyeMessage(GoodbyeMessage goodbye, Data data)
{
    Relay *relay = GetRelayByNumber(goodbye.dead_relay_number);
    if (relay == NULL) throw RelayStateException("no such relay");

    relay->RecordGoodbyeMessage(goodbye);
    RecordRelayDeath(relay, data, SAID_GOODBYE);
}

void RelayState::ProcessSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    Relay *dead_relay = GetRelayByNumber(secret_recovery_message.dead_relay_number);
    Relay *successor = GetRelayByNumber(secret_recovery_message.successor_number);

    RecordSecretRecoveryMessageTaskCompleted(secret_recovery_message);
    dead_relay->RecordSecretRecoveryMessage(secret_recovery_message);

    AssignCompleteSuccessionTaskIfSuccessionAttemptHasFourMessages(dead_relay, successor);
}

void RelayState::AssignCompleteSuccessionTaskIfSuccessionAttemptHasFourMessages(Relay *dead_relay, Relay *successor)
{
    auto &attempt = dead_relay->succession_attempts[successor->number];

    if (attempt.HasFourRecoveryMessages())
    {
        auto four_messages = attempt.GetFourSecretRecoveryMessages();
        Task task(SEND_SUCCESSION_COMPLETED_MESSAGE, four_messages.GetHash160(), 0);
        successor->tasks.push_back(task);
    }
}

void RelayState::RecordSecretRecoveryMessageTaskCompleted(SecretRecoveryMessage &secret_recovery_message)
{
    Relay *quarter_holder = GetRelayByNumber(secret_recovery_message.quarter_holder_number);

    Task task(SEND_SECRET_RECOVERY_MESSAGE,
              Death(secret_recovery_message.dead_relay_number),
              secret_recovery_message.position_of_quarter_holder);

    EraseEntryFromVector(task, quarter_holder->tasks);
}

void RelayState::ProcessSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data)
{
    RemoveCompleteSuccessionTaskFromSuccessor(complaint, data);
    RecordSentComplaint(complaint.GetComplainer(data));

    auto dead_relay = complaint.GetDeadRelay(data);
    dead_relay->RecordSecretRecoveryComplaintAndRejectBadRecoveryMessage(complaint);

    AssignSecretRecoveryTaskToKeyQuarterHolder(dead_relay, complaint.position_of_quarter_holder);

    auto key_quarter_holder = complaint.GetSecretSender(data);
    RecordRelayDeath(key_quarter_holder, data, MISBEHAVED);
}

void RelayState::RemoveCompleteSuccessionTaskFromSuccessor(SecretRecoveryComplaint &complaint, Data data)
{
    auto successor = GetRelayByNumber(complaint.successor_number);
    auto dead_relay = complaint.GetDeadRelay(data);
    auto &attempt = dead_relay->succession_attempts[complaint.successor_number];
    auto hash_of_four_messages = attempt.GetFourSecretRecoveryMessages().GetHash160();
    Task task(SEND_SUCCESSION_COMPLETED_MESSAGE, hash_of_four_messages);
    EraseEntryFromVector(task, successor->tasks);
}

void RelayState::RecordRelayDeath(Relay *dead_relay, Data data, uint8_t reason)
{
    dead_relay->status = reason;
    AssignSecretRecoveryTasksToKeyQuarterHolders(dead_relay);
    AssignNewSuccessorsAfterRelayDeath(dead_relay);
    ReassignSecretRecoveryTasksToQuarterHoldersAfterSuccessorDeath(dead_relay);
    AssignNewQuarterHoldersIfNecessaryAfterRelayDeath(dead_relay);
}

void RelayState::AssignNewSuccessorsAfterRelayDeath(Relay *dead_relay)
{
    for (auto &relay : relays)
        if (relay.current_successor_number == dead_relay->number)
        {
            relay.previous_successors.push_back(dead_relay->number);
            AssignSuccessorToRelay(&relay);
        }
}

void RelayState::ReassignSecretRecoveryTasksToQuarterHoldersAfterSuccessorDeath(Relay *dead_successor)
{
    for (auto &relay : relays)
    {
        if (relay.previous_successors.size() > 0 and relay.previous_successors.back() == dead_successor->number)
        {
            if (relay.current_successor_number != 0)
                AssignSecretRecoveryTasksToKeyQuarterHolders(&relay);
        }
    }
}

void RelayState::AssignNewQuarterHoldersIfNecessaryAfterRelayDeath(Relay *dead_relay)
{
    if (dead_relay->current_successor_number != 0)
        return;

    for (auto &relay : relays)
        for (uint32_t position = 0; position < 4; position++)
            if (relay.key_quarter_holders[position] == dead_relay->number)
                AssignReplacementQuarterHolderToRelay(&relay, position);
}

void RelayState::AssignReplacementQuarterHolderToRelay(Relay *relay, uint32_t position)
{
    AssignKeyQuarterHolderToRelay(*relay, position);
    relay->key_quarter_locations[position] = KeyQuarterLocation();
}

void RelayState::ProcessDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration, Data data)
{
    std::string message_type = data.msgdata[duration.message_hash]["type"];

    if (IsDeath(duration.message_hash))
    {
        ProcessDurationWithoutRelayResponseAfterDeath(duration.relay_number, data);
    }

    else if (message_type == "secret_recovery_failure")
    {
        SecretRecoveryFailureMessage failure_message = data.msgdata[duration.message_hash][message_type];
        ProcessDurationWithoutRelayResponseAfterSecretRecoveryFailureMessage(failure_message,
                                                                             duration.relay_number, data);
    }
}

void RelayState::ProcessDurationWithoutRelayResponseAfterDeath(uint64_t dead_relay_number, Data data)
{
    Relay *key_quarter_holder = GetRelayByNumber(dead_relay_number);
    RecordRelayDeath(key_quarter_holder, data, NOT_RESPONDING);
}

void RelayState::ProcessDurationWithoutRelayResponseAfterSecretRecoveryFailureMessage(
        SecretRecoveryFailureMessage failure_message, uint64_t relay_number, Data data)
{
    Relay *key_quarter_holder = GetRelayByNumber(relay_number);
    RecordRelayDeath(key_quarter_holder, data, NOT_RESPONDING);
}

void RelayState::ProcessRecoveryFailureAuditMessage(RecoveryFailureAuditMessage audit_message, Data data)
{
    auto dead_relay = audit_message.GetDeadRelay(data);
    dead_relay->RecordRecoveryFailureAuditMessage(audit_message);

    RecordRecoveryFailureAuditMessageTaskCompleted(audit_message, data);
    ExamineAuditMessageToDetermineWrongdoer(audit_message, dead_relay, data);
}

void RelayState::RecordRecoveryFailureAuditMessageTaskCompleted(RecoveryFailureAuditMessage audit_message, Data data)
{
    Relay *key_quarter_holder = GetRelayByNumber(audit_message.quarter_holder_number);
    Task task(SEND_RECOVERY_FAILURE_AUDIT_MESSAGE, audit_message.failure_message_hash, audit_message.position_of_quarter_holder);
    EraseEntryFromVector(task, key_quarter_holder->tasks);
}

void RelayState::ExamineAuditMessageToDetermineWrongdoer(RecoveryFailureAuditMessage audit_message,
                                                         Relay *dead_relay, Data data)
{
    auto quarter_holder = GetRelayByNumber(audit_message.quarter_holder_number);
    if (not audit_message.ContainsCorrectData(data))
    {
        RecordRelayDeath(quarter_holder, data, MISBEHAVED);
        data.msgdata[audit_message.failure_message_hash]["bad_quarter_holder_found"] = true;
        return;
    }
    auto &succession_attempt = dead_relay->succession_attempts[audit_message.successor_number];
    auto &audit = succession_attempt.audits[audit_message.failure_message_hash];
    if (audit.FourAuditMessagesHaveBeenReceived() and
            not data.msgdata[audit_message.failure_message_hash]["bad_quarter_holder_found"])
        DetermineWhetherSuccessorOrKeySharerIsAtFaultInSecretRecoveryFailure(dead_relay,
                                                                             audit_message.failure_message_hash, data);
}

void RelayState::DetermineWhetherSuccessorOrKeySharerIsAtFaultInSecretRecoveryFailure(Relay *dead_relay,
                                                                                      uint160 failure_message_hash,
                                                                                      Data data)
{
    if (SuccessorWasLyingAboutSumOfRecoveredSharedSecretQuarters(dead_relay, failure_message_hash, data))
    {
        auto successor = data.relay_state->GetRelayByNumber(dead_relay->SuccessorNumberFromLatestSuccessionAttempt(data));
        RecordRelayDeath(successor, data, MISBEHAVED);
    }
    else
    {
        SecretRecoveryFailureMessage failure_message = data.GetMessage(failure_message_hash);
        RecordRelayDeath(failure_message.GetKeySharer(data), data, MISBEHAVED);
    }
}

bool RelayState::SuccessorWasLyingAboutSumOfRecoveredSharedSecretQuarters(Relay *dead_relay,
                                                                          uint160 failure_message_hash, Data data)
{
    SecretRecoveryFailureMessage failure_message = data.GetMessage(failure_message_hash);
    auto public_key_twentyfourth = failure_message.GetKeyTwentyFourth(data);
    Point sum_of_shared_secret_quarters(CBigNum(0));
    auto &succession_attempt = dead_relay->succession_attempts[failure_message.successor_number];
    auto &audit = succession_attempt.audits[failure_message_hash];

    for (auto audit_message_hash : audit.audit_message_hashes)
    {
        RecoveryFailureAuditMessage audit_message = data.GetMessage(audit_message_hash);
        sum_of_shared_secret_quarters += audit_message.private_receiving_key_quarter * public_key_twentyfourth;
    }
    return sum_of_shared_secret_quarters != failure_message.sum_of_decrypted_shared_secret_quarters;
}

void RelayState::ProcessSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message, Data data)
{
    auto dead_relay = failure_message.GetDeadRelay(data);

    dead_relay->RecordSecretRecoveryFailureMessage(failure_message);
    AssignRecoveryFailureAuditTasksToQuarterHolders(failure_message, data);
}

void RelayState::AssignRecoveryFailureAuditTasksToQuarterHolders(SecretRecoveryFailureMessage failure_message, Data data)
{
    auto quarter_holders = failure_message.GetQuarterHolders(data);
    uint160 failure_message_hash = failure_message.GetHash160();

    for (uint32_t position = 0; position < 4; position++)
    {
        auto quarter_holder = quarter_holders[position];
        Task task(SEND_RECOVERY_FAILURE_AUDIT_MESSAGE, failure_message_hash, position);
        quarter_holder->tasks.push_back(task);
    }
}

bool RelayState::ContainsRelayWithNumber(uint64_t relay_number)
{
    return GetRelayByNumber(relay_number) != NULL;
}

void RelayState::ProcessSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message, Data data)
{
    auto dead_relay = GetRelayByNumber(succession_completed_message.dead_relay_number);
    dead_relay->RecordSuccessionCompletedMessage(succession_completed_message);
    auto successor = GetRelayByNumber(succession_completed_message.successor_number);

    RemoveCompleteSuccessionTaskFromSuccessor(succession_completed_message);
    TransferInheritableTasksToSuccessor(dead_relay->number, successor->number);
    ReplaceDeadRelayWithSuccessorInQuarterHolderRoles(dead_relay->number, successor->number);
}

void RelayState::RemoveCompleteSuccessionTaskFromSuccessor(SuccessionCompletedMessage &succession_completed_message)
{
    auto successor = GetRelayByNumber(succession_completed_message.successor_number);
    auto hash_of_four_messages = succession_completed_message.GetFourSecretRecoveryMessages().GetHash160();
    Task task(SEND_SUCCESSION_COMPLETED_MESSAGE, hash_of_four_messages);
    EraseEntryFromVector(task, successor->tasks);
}

void RelayState::ProcessSuccessionCompletedAuditMessage(SuccessionCompletedAuditMessage &audit_message, Data data)
{
    auto dead_relay = GetRelayByNumber(audit_message.dead_relay_number);
    dead_relay->RecordSuccessionCompletedAuditMessage(audit_message);
}

KeyDistributionAuditorSelection RelayState::GenerateKeyDistributionAuditorSelection(Relay *key_sharer)
{
    KeyDistributionAuditorSelection auditor_selection;
    auditor_selection.Populate(key_sharer, this);
    return auditor_selection;
}

void RelayState::ProcessKeyDistributionAuditMessage(KeyDistributionAuditMessage audit_message)
{
    auto key_sharer = GetRelayByNumber(audit_message.relay_number);
    key_sharer->hashes.key_distribution_audit_message_hash = audit_message.GetHash160();
    RemoveKeyDistributionAuditTask(audit_message);
}

void RelayState::RemoveKeyDistributionAuditTask(KeyDistributionAuditMessage &audit_message)
{
    auto key_sharer = GetRelayByNumber(audit_message.relay_number);
    Task task(SEND_KEY_DISTRIBUTION_AUDIT_MESSAGE, audit_message.key_distribution_message_hash, audit_message.position);
    EraseEntryFromVector(task, key_sharer->tasks);
}

void RelayState::ProcessMinedCreditMessage(MinedCreditMessage &msg, Data data)
{
    msg.hash_list.RecoverFullHashes(data.msgdata);
    uint160 mined_credit_message_hash = msg.GetHash160();

    for (auto message_hash : msg.hash_list.full_hashes)
    {
        std::string message_type = data.MessageType(message_hash);

        if (message_type == "relay_join")
            MarkRelayJoinMessageAsEncoded(message_hash, data);

        if (message_type == "key_distribution")
            MarkKeyDistributionMessageAsEncoded(message_hash, data);

        if (message_type == "succession_completed")
            MarkSuccessionCompletedMessageAsEncodedInMinedCreditMessage(message_hash, mined_credit_message_hash, data);
    }

    AssignKeyDistributionTasks();
    AssignKeyDistributionAuditTasks();
    AssignSuccessionCompletedAuditTasks();
    latest_mined_credit_message_hash = msg.GetHash160();
}

void RelayState::AssignKeyDistributionTasks()
{
    for (auto &relay : relays)
    {
        if (RelayShouldSendAKeyDistributionMessage(relay))
        {
            AssignKeyDistributionTaskToRelay(relay);
        }
    }
}

void RelayState::AssignKeyDistributionTaskToRelay(Relay &relay)
{
    auto positions = PositionsForWhichRelayShouldSendAKeyDistributionMessage(relay);
    vector<Task> tasks;

    if (positions.size() == 4)
        tasks.push_back(Task(SEND_KEY_DISTRIBUTION_MESSAGE, relay.hashes.join_message_hash, ALL_POSITIONS));
    else
        for (auto position : positions)
            tasks.push_back(Task(SEND_KEY_DISTRIBUTION_MESSAGE, relay.hashes.join_message_hash, position));

    for (auto task : tasks)
        if (not VectorContainsEntry(relay.tasks, task))
            relay.tasks.push_back(task);
}

vector<uint32_t> RelayState::PositionsForWhichRelayShouldSendAKeyDistributionMessage(Relay &relay)
{
    vector<uint32_t> positions;
    for (uint32_t position = 0; position < 4; position++)
    {
        if (relay.key_quarter_locations[position].message_hash == 0)
            positions.push_back(position);
    }
    return positions;
}

void RelayState::AssignKeyDistributionAuditTasks()
{
    for (auto &relay : relays)
    {
        if (RelayShouldSendAKeyDistributionAuditMessage(relay))
        {
            AssignKeyDistributionAuditTaskToRelay(relay);
        }
    }
}

void RelayState::AssignKeyDistributionAuditTaskToRelay(Relay &relay)
{
    vector<uint8_t> positions;
    for (auto &location : relay.key_quarter_locations)
    {
        if (location.message_was_encoded and location.audit_message_hash == 0 and not location.key_quarter_was_recovered)
        {
            positions.push_back((uint8_t)location.position);
        }
    }
    if (positions.size() == 4)
    {
        Task task(SEND_KEY_DISTRIBUTION_AUDIT_MESSAGE, relay.key_quarter_locations[0].message_hash, ALL_POSITIONS);
        if (not VectorContainsEntry(relay.tasks, task))
            relay.tasks.push_back(task);
    }
}

void RelayState::AssignSuccessionCompletedAuditTasks()
{
    for (auto &relay : relays)
        if (RelayShouldSendASuccessionCompletedAuditMessage(relay))
            AssignSuccessionCompletedAuditTasksToRelay(relay);
}

bool RelayState::RelayShouldSendASuccessionCompletedAuditMessage(Relay &relay)
{
    for (auto &candidate_predecessor : relays)
        if (candidate_predecessor.succession_attempts.count(relay.number))
        {
            auto &attempt = candidate_predecessor.succession_attempts[relay.number];
            if (SuccessionCompletedAuditMessageShouldBeSentForSuccessionAttempt(attempt))
                return true;
        }
    return false;
}

bool RelayState::SuccessionCompletedAuditMessageShouldBeSentForSuccessionAttempt(SuccessionAttempt &attempt)
{
    if (attempt.succession_completed_message_hash == 0)
        return false;
    if (attempt.hash_of_message_containing_succession_completed_message == 0)
        return false;
    if (attempt.succession_completed_audit_message_hash != 0)
        return false;;
    return true;
}

void RelayState::AssignSuccessionCompletedAuditTasksToRelay(Relay &relay)
{
    for (auto &candidate_predecessor : relays)
        if (candidate_predecessor.succession_attempts.count(relay.number))
        {
            auto &attempt = candidate_predecessor.succession_attempts[relay.number];
            if (SuccessionCompletedAuditMessageShouldBeSentForSuccessionAttempt(attempt))
                AssignSuccessionCompletedAuditTaskToRelay(attempt.succession_completed_message_hash, relay);
        }
}

void RelayState::AssignSuccessionCompletedAuditTaskToRelay(uint160 succession_completed_message_hash, Relay &relay)
{
    Task task(SEND_SUCCESSION_COMPLETED_AUDIT_MESSAGE, succession_completed_message_hash);
    relay.tasks.push_back((task));
}

bool RelayState::RelayShouldSendAKeyDistributionAuditMessage(Relay &relay)
{
    if (relay.NumberOfKeyQuarterHoldersAssigned() == 0)
        return false;
    if (RelayHasAnEncodedKeyDistributionMessageWithNoCorrespondingAuditMessage(relay))
        return true;;
    return false;
}

bool RelayState::RelayHasAnEncodedKeyDistributionMessageWithNoCorrespondingAuditMessage(Relay &relay)
{
    for (auto &location : relay.key_quarter_locations)
        if (location.message_was_encoded and location.audit_message_hash == 0 and not location.key_quarter_was_recovered)
            return true;
    return false;
}

bool RelayState::RelayShouldSendAKeyDistributionMessage(Relay &relay)
{
    if (relay.NumberOfKeyQuarterHoldersAssigned() == 0)
        return false;
    if (RelayHasFourLivingQuarterHoldersWhoHaveReceivedKeyQuarters(relay))
        return false;

    if (RelayHasFourLivingKeyQuarterHoldersWithEncodedJoinMessages(relay) and
            RelayHasALivingQuarterHolderWithAnEncodedJoinMessageWhoHasNotReceivedAKeyQuarter(relay))
        return true;;
    return false;
}

bool RelayState::RelayHasALivingQuarterHolderWithAnEncodedJoinMessageWhoHasNotReceivedAKeyQuarter(Relay &relay)
{
    for (uint32_t position = 0; position < 4; position++)
    {
        if (RelayIsAliveAndHasAnEncodedJoinMessage(relay.key_quarter_holders[position]))
        {
            if (relay.key_quarter_locations[position].message_hash == 0)
                return true;
        }
    }
    return false;
}

bool RelayState::RelayIsAliveAndHasAnEncodedJoinMessage(uint64_t relay_number)
{
    if (not ContainsRelayWithNumber(relay_number))
        return false;

    auto relay = GetRelayByNumber(relay_number);
    return relay->status == ALIVE and relay->join_message_was_encoded;
}

bool RelayState::RelayHasFourLivingKeyQuarterHolders(Relay &relay)
{
    for (auto quarter_holder_number : relay.key_quarter_holders)
    {
        auto quarter_holder = GetRelayByNumber(quarter_holder_number);
        if (quarter_holder == NULL or quarter_holder->status != ALIVE)
            return false;
    }
    return true;
}

bool RelayState::RelayHasFourLivingKeyQuarterHoldersWithEncodedJoinMessages(Relay &relay)
{
    for (auto quarter_holder_number : relay.key_quarter_holders)
    {
        auto quarter_holder = GetRelayByNumber(quarter_holder_number);
        if (quarter_holder == NULL or quarter_holder->status != ALIVE or not quarter_holder->join_message_was_encoded)
            return false;
    }
    return true;
}

bool RelayState::RelayHasFourLivingQuarterHoldersWhoHaveReceivedKeyQuarters(Relay &relay)
{
    if (not RelayHasFourLivingKeyQuarterHolders(relay))
        return false;
    for (auto key_quarter_location : relay.key_quarter_locations)
        if (key_quarter_location.message_hash == 0)
            return false;
    return true;
}

void RelayState::MarkRelayJoinMessageAsEncoded(uint160 join_message_hash, Data data)
{
    auto relay = GetRelayByJoinHash(join_message_hash);
    relay->join_message_was_encoded = true;
}

void RelayState::MarkKeyDistributionMessageAsEncoded(uint160 key_distribution_message_hash, Data data)
{
    KeyDistributionMessage key_distribution_message = data.GetMessage(key_distribution_message_hash);
    auto relay = GetRelayByNumber(key_distribution_message.relay_number);
    for (auto &location : relay->key_quarter_locations)
        location.message_was_encoded = true;
}

void RelayState::MarkSuccessionCompletedMessageAsEncodedInMinedCreditMessage(uint160 succession_completed_message_hash,
                                                                             uint160 mined_credit_message_hash,
                                                                             Data data)
{
    SuccessionCompletedMessage succession_completed_message = data.GetMessage(succession_completed_message_hash);
    auto dead_relay = GetRelayByNumber(succession_completed_message.dead_relay_number);
    auto &attempt = dead_relay->succession_attempts[succession_completed_message.successor_number];
    attempt.hash_of_message_containing_succession_completed_message = mined_credit_message_hash;
}

void RelayState::ProcessSuccessionCompletedAuditComplaint(SuccessionCompletedAuditComplaint complaint, Data data)
{
    auto dead_relay = GetRelayByNumber(complaint.dead_relay_number);
    auto &attempt = dead_relay->succession_attempts[complaint.successor_number];
    attempt.succession_completed_audit_complaints.push_back(complaint.GetHash160());

    auto auditor = GetRelayByNumber(complaint.auditor_number);
    RecordSentComplaint(auditor);

    RestorePreviousKeyQuarterLocationsAfterSuccessionCompletedAuditComplaint(complaint, data);

    auto successor = GetRelayByNumber(complaint.successor_number);
    RecordRelayDeath(successor, data, MISBEHAVED);
}

void RelayState::RestorePreviousKeyQuarterLocationsAfterSuccessionCompletedAuditComplaint(
        SuccessionCompletedAuditComplaint &complaint, Data data)
{
    auto succession_completed_message_hash = complaint.succession_completed_message_hash;
    SuccessionCompletedMessage succession_completed = data.GetMessage(succession_completed_message_hash);

    for (auto &relay : relays)
        if (relay.previous_key_quarter_locations.size() > 0)
            for (auto &location : relay.key_quarter_locations)
                if (location.message_hash == succession_completed_message_hash)
                {
                    RestorePreviousKeyQuarterLocationsForRelay(relay, succession_completed_message_hash, data);
                    break;
                }
}

void RelayState::RestorePreviousKeyQuarterLocationsForRelay(Relay &relay, uint160 bad_message_hash, Data data)
{
    vector<uint160> restored_locations;
    for (uint32_t key_quarter_position = 0; key_quarter_position < 4; key_quarter_position++)
    {
        auto &location = relay.key_quarter_locations[key_quarter_position];
        if (location.message_hash == bad_message_hash)
        {
            uint160 previous_location = GetPreviousLocationForRelaysKeyQuarter(relay, key_quarter_position, data);
            location.message_hash = previous_location;
            location.position = GetPositionOfRelaysKeyQuarterInMessage(relay, key_quarter_position,
                                                                       previous_location, data);
            if (not VectorContainsEntry(restored_locations, previous_location))
                restored_locations.push_back(previous_location);
        }
    }
    for (auto restored_location : restored_locations)
        EraseEntryFromVector(restored_location, relay.previous_key_quarter_locations);
}

uint160 RelayState::GetPreviousLocationForRelaysKeyQuarter(Relay &relay, uint32_t key_quarter_position, Data data)
{
    for (uint160 previous_location_hash : boost::adaptors::reverse(relay.previous_key_quarter_locations))
    {
        std::string message_type = data.MessageType(previous_location_hash);
        if (message_type == "key_distribution")
            return previous_location_hash;
        if (message_type == "succession_completed")
        {
            SuccessionCompletedMessage succession_completed = data.GetMessage(previous_location_hash);
            for (uint32_t i = 0; i < succession_completed.key_quarter_sharers.size(); i++)
            {
                if (succession_completed.key_quarter_sharers[i] == relay.number)
                    if (succession_completed.key_quarter_positions[i] == key_quarter_position)
                        return previous_location_hash;
            }
        }
    }
    return 0;
}

uint32_t RelayState::GetPositionOfRelaysKeyQuarterInMessage(Relay &relay, uint32_t key_quarter_position,
                                                            uint160 previous_location_hash, Data data)
{
    std::string message_type = data.MessageType(previous_location_hash);
    if (message_type == "key_distribution")
        return key_quarter_position;
    if (message_type == "succession_completed")
    {
        SuccessionCompletedMessage succession_completed = data.GetMessage(previous_location_hash);
        for (uint32_t i = 0; i < succession_completed.key_quarter_sharers.size(); i++)
        {
            if (succession_completed.key_quarter_sharers[i] == relay.number)
                if (succession_completed.key_quarter_positions[i] == key_quarter_position)
                    return i;
        }
    }
    return 0;
}

std::vector<uint64_t> RelayState::ChooseRelaysForDepositAddressRequest(uint160 relay_chooser, uint64_t number_of_relays)
{
    vector<uint64_t> relay_numbers;

    while (relay_numbers.size() < number_of_relays)
    {
        relay_chooser = HashUint160(relay_chooser);
        auto &relay = relays[relay_chooser.GetLow64() % relays.size()];
        if (VectorContainsEntry(relay_numbers, relay.number) or relay.status != ALIVE)
            continue;
        relay_numbers.push_back(relay.number);
    }
    return relay_numbers;
}
