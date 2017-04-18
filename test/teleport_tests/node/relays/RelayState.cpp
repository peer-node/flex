#include <src/vector_tools.h>
#include <src/credits/creditsign.h>
#include "RelayState.h"
#include "SuccessionCompletedAuditMessage.h"


using std::vector;

#include "log.h"
#define LOG_CATEGORY "RelayState.cpp"

vector<uint64_t> ChooseFromList(vector<uint64_t> list, uint64_t number_required, uint160 chooser)
{
    vector<uint64_t> choices;
    while (choices.size() < number_required and list.size() > 0)
    {
        chooser = HashUint160(chooser);
        auto chosen_position = chooser.GetLow64() % list.size();
        choices.push_back(list[chosen_position]);
        list.erase(list.begin() + chosen_position);
    }
    return choices;
}

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
    uint32_t quarter_holder_position = 1;
    for (int64_t n = (int64_t)relay.number - 11; n >= 0 and n >= relays.front().number; n--)
        if (RelayIsASuitableQuarterHolder(&relay, n))
        {
            relay.key_quarter_holders[quarter_holder_position] = (uint64_t)n;
            quarter_holder_position++;
            if (quarter_holder_position == 4)
                return;
        }
    for (int64_t n = (int64_t)relay.number + 1; n <= relays.back().number; n++)
        if (RelayIsASuitableQuarterHolder(&relay, n))
        {
            relay.key_quarter_holders[quarter_holder_position] = (uint64_t)n;
            quarter_holder_position++;
            if (quarter_holder_position == 4)
                return;
        }
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
    RemoveKeyDistributionTask(relay);
}

void RelayState::RecordKeyDistributionMessage(Relay *relay, uint160 key_distribution_message_hash)
{
    for (uint32_t position = 0; position < 4; position++)
    {
        relay->key_quarter_locations[position].message_hash = key_distribution_message_hash;
        relay->key_quarter_locations[position].position = position;
    }
}

void RelayState::RemoveKeyDistributionTask(Relay *relay)
{
    Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay->hashes.join_message_hash, ALL_POSITIONS);
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

void RelayState::AssignSecretRecoveryTasksToKeyQuarterHolders(Relay *dead_relay, Data data)
{
    for (uint32_t position = 0; position < 4; position++)
    {
        auto key_quarter_holder = GetRelayByNumber(dead_relay->key_quarter_holders[position]);
        Task task(SEND_SECRET_RECOVERY_MESSAGE, Death(dead_relay->number), position);
        key_quarter_holder->tasks.push_back(task);
    }
}

void RelayState::TransferTasksToSuccessor(uint64_t dead_relay_number, uint64_t successor_number)
{
    Relay *successor = GetRelayByNumber(successor_number);
    Relay *dead_relay = GetRelayByNumber(dead_relay_number);
    if (dead_relay == NULL or successor == NULL) throw RelayStateException("no such relay");

    successor->tasks = ConcatenateVectors(successor->tasks, dead_relay->tasks);
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

    if (message_type == "key_distribution_audit")
        ProcessDurationWithoutResponseAfterKeyDistributionAuditMessage(
                data.msgdata[duration.message_hash][message_type], data);

    else if (message_type == "four_secret_recovery_messages")
        ProcessDurationWithoutResponseAfterFourSecretRecoveryMessages(data.msgdata[duration.message_hash][message_type],
                                                                      data);

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
    Relay *key_quarter_holder = complaint.GetSecretSender(data);
    Relay *dead_relay = complaint.GetDeadRelay(data);

    auto recovery_message = complaint.GetSecretRecoveryMessage(data);
    dead_relay->RecordSecretRecoveryComplaint(complaint);

    Task task(SEND_SECRET_RECOVERY_MESSAGE,
              Death(recovery_message.dead_relay_number),
              recovery_message.position_of_quarter_holder);

    key_quarter_holder->tasks.push_back(task);
    RecordRelayDeath(key_quarter_holder, data, MISBEHAVED);
}

void RelayState::RecordRelayDeath(Relay *dead_relay, Data data, uint8_t reason)
{
    dead_relay->status = reason;
    AssignSecretRecoveryTasksToKeyQuarterHolders(dead_relay, data);
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
    TransferTasksToSuccessor(dead_relay->number, successor->number);
    ReplaceDeadRelayWithSuccessorInQuarterHolderRoles(dead_relay->number, successor->number);
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

    for (auto message_hash : msg.hash_list.full_hashes)
    {
        if (data.MessageType(message_hash) == "relay_join")
            MarkRelayJoinMessageAsEncoded(message_hash, data);

        if (data.MessageType(message_hash) == "key_distribution")
            MarkKeyDistributionMessageAsEncoded(message_hash, data);
    }

    AssignKeyDistributionTasks();
    AssignKeyDistributionAuditTasks();
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
    Task task(SEND_KEY_DISTRIBUTION_MESSAGE, relay.hashes.join_message_hash, ALL_POSITIONS);
    if (not VectorContainsEntry(relay.tasks, task))
        relay.tasks.push_back(task);
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
    if (RelayHasSharedAKeyQuarterWithAQuarterHolderWhoIsNowDeadAndHasNoSuccessor(relay) and
            NewQuarterHoldersAreAvailableToReplaceDeadOnesWithoutSuccessors(relay))
        return true;
    if (RelayHasFourLivingQuarterHoldersWithEncodedJoinMessagesWhoHaveNotReceivedKeyQuarters(relay))
        return true;;
    return false;
}

bool RelayState::RelayHasFourLivingQuarterHoldersWithEncodedJoinMessagesWhoHaveNotReceivedKeyQuarters(Relay &relay)
{
    if (not RelayHasFourLivingKeyQuarterHoldersWithEncodedJoinMessages(relay))
        return false;

    for (auto key_quarter_location : relay.key_quarter_locations)
        if (key_quarter_location.message_hash != 0)
            return false;
    return true;
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

bool RelayState::RelayHasSharedAKeyQuarterWithAQuarterHolderWhoIsNowDeadAndHasNoSuccessor(Relay &relay)
{
    for (uint32_t i = 0; i < 4; i++)
    {
        auto quarter_holder = GetRelayByNumber(relay.key_quarter_holders[i]);
        if (quarter_holder != NULL and quarter_holder->status != ALIVE)
        {
            if (relay.key_quarter_locations[i].message_hash != 0 and quarter_holder->current_successor_number == 0)
                return true;
        }
    }
    return false;
}

bool RelayState::NewQuarterHoldersAreAvailableToReplaceDeadOnesWithoutSuccessors(Relay &relay)
{
    return false;
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