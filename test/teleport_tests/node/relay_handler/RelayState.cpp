#include <src/vector_tools.h>
#include <src/credits/creditsign.h>
#include "RelayState.h"


#include "log.h"
#define LOG_CATEGORY "RelayState.cpp"



void RelayState::ProcessRelayJoinMessage(RelayJoinMessage relay_join_message)
{
    ThrowExceptionIfMinedCreditMessageHashWasAlreadyUsed(relay_join_message);
    Relay relay = GenerateRelayFromRelayJoinMessage(relay_join_message);
    relays.push_back(relay);
    maps_are_up_to_date = false;
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

void RelayState::AssignRemainingQuarterKeyHoldersToRelay(Relay &relay, uint160 encoding_message_hash)
{
    uint64_t tries = 0;
    while (relay.holders.key_quarter_holders.size() < 4)
    {
        uint64_t quarter_key_holder = SelectKeyHolderWhichIsntAlreadyBeingUsed(relay,
                                                                               encoding_message_hash + (tries++));

        bool candidate_quarter_key_holder_has_this_relay_as_quarter_key_holder
                = VectorContainsEntry(GetRelayByNumber(quarter_key_holder)->holders.key_quarter_holders, relay.number);

        if (candidate_quarter_key_holder_has_this_relay_as_quarter_key_holder)
            continue;
        relay.holders.key_quarter_holders.push_back(quarter_key_holder);
    }
}

uint64_t RelayState::KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed)
{
    uint64_t chosen_relay_number = SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, random_seed);
    while (chosen_relay_number <= relay.number)
    {
        random_seed = HashUint160(random_seed);
        chosen_relay_number = SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, random_seed);
    }
    return chosen_relay_number;
}

uint64_t RelayState::SelectKeyHolderWhichIsntAlreadyBeingUsed(Relay &relay, uint160 random_seed)
{
    bool found = false;
    uint64_t relay_position = 0;
    while (not found)
    {
        random_seed = HashUint160(random_seed);
        relay_position = (CBigNum(random_seed) % CBigNum(relays.size())).getulong();
        if (relays[relay_position].number != relay.number and not KeyPartHolderIsAlreadyBeingUsed(relay, relays[relay_position]))
        {
            found = true;
        }
    }
    return relays[relay_position].number;
}

bool RelayState::KeyPartHolderIsAlreadyBeingUsed(Relay &key_distributor, Relay &key_part_holder)
{
    for (auto key_part_holder_group : key_distributor.KeyPartHolderGroups())
    {
        for (auto number_of_existing_key_part_holder : key_part_holder_group)
            if (number_of_existing_key_part_holder == key_part_holder.number)
            {
                return true;
            }
    }
    return false;
}

void RelayState::AssignRemainingKeySixteenthHoldersToRelay(Relay &relay, uint160 encoding_message_hash)
{
    while (relay.holders.first_set_of_key_sixteenth_holders.size() < 16)
        relay.holders.first_set_of_key_sixteenth_holders.push_back(SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, encoding_message_hash));
    while (relay.holders.second_set_of_key_sixteenth_holders.size() < 16)
        relay.holders.second_set_of_key_sixteenth_holders.push_back(SelectKeyHolderWhichIsntAlreadyBeingUsed(relay, encoding_message_hash));
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

bool RelayState::ThereAreEnoughRelaysToAssignKeyPartHolders(Relay &relay)
{
    uint64_t number_of_relays_that_joined_later = NumberOfRelaysThatJoinedLaterThan(relay);
    uint64_t number_of_relays_that_joined_before = NumberOfRelaysThatJoinedBefore(relay);

    return number_of_relays_that_joined_later >= 3 and
            number_of_relays_that_joined_before + number_of_relays_that_joined_later >= 36;
}

void RelayState::RemoveKeyPartHolders(Relay &relay)
{
    relay.holders.key_quarter_holders.resize(0);
    relay.holders.first_set_of_key_sixteenth_holders.resize(0);
    relay.holders.second_set_of_key_sixteenth_holders.resize(0);
}

bool RelayState::AssignKeyPartHoldersToRelay(Relay &relay, uint160 encoding_message_hash)
{
    if (not ThereAreEnoughRelaysToAssignKeyPartHolders(relay))
        return false;
    RemoveKeyPartHolders(relay);
    AssignKeySixteenthAndQuarterHoldersWhoJoinedLater(relay, encoding_message_hash);
    AssignRemainingQuarterKeyHoldersToRelay(relay, encoding_message_hash);
    AssignRemainingKeySixteenthHoldersToRelay(relay, encoding_message_hash);
    return true;
}

void RelayState::AssignKeySixteenthAndQuarterHoldersWhoJoinedLater(Relay &relay, uint160 encoding_message_hash)
{
    uint64_t key_part_holder = KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(relay, encoding_message_hash);
    relay.holders.key_quarter_holders.push_back(key_part_holder);
    key_part_holder = KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(relay, encoding_message_hash);
    relay.holders.first_set_of_key_sixteenth_holders.push_back(key_part_holder);
    key_part_holder = KeyPartHolderWhoJoinedLaterAndIsntAlreadyBeingUsed(relay, encoding_message_hash);
    relay.holders.second_set_of_key_sixteenth_holders.push_back(key_part_holder);
}

uint64_t RelayState::NumberOfRelaysThatJoinedLaterThan(Relay &relay)
{
    uint64_t count = 0;
    for (uint64_t n = relays.size(); n > 0 and relays[n - 1].number > relay.number; n--)
        count++;
    return count;
}

uint64_t RelayState::NumberOfRelaysThatJoinedBefore(Relay &relay)
{
    uint64_t count = 0;
    for (uint64_t n = 0; n < relays.size() and relays[n].number < relay.number; n++)
        count++;
    return count;
}

std::vector<uint64_t> RelayState::GetKeyQuarterHoldersAsListOf16RelayNumbers(uint64_t relay_number)
{
    Relay *relay = GetRelayByNumber(relay_number);
    if (relay == NULL) throw RelayStateException("no such relay");
    std::vector<uint64_t> recipient_relay_numbers;
    for (auto quarter_key_holder : relay->holders.key_quarter_holders)
    {
        for (uint64_t i = 0; i < 4; i++)
            recipient_relay_numbers.push_back(quarter_key_holder);
    }
    return recipient_relay_numbers;
}

std::vector<uint64_t>
RelayState::GetKeySixteenthHoldersAsListOf16RelayNumbers(uint64_t relay_number, uint64_t first_or_second_set)
{
    Relay *relay = GetRelayByNumber(relay_number);
    if (relay == NULL) throw RelayStateException("no such relay");

    return (first_or_second_set == 1) ? relay->holders.first_set_of_key_sixteenth_holders
                                                                   : relay->holders.second_set_of_key_sixteenth_holders;
}

void RelayState::ProcessKeyDistributionMessage(KeyDistributionMessage key_distribution_message)
{
    Relay *relay = GetRelayByJoinHash(key_distribution_message.relay_join_hash);
    if (relay == NULL) throw RelayStateException("no such relay");
    if (relay->hashes.key_distribution_message_hash != 0)
        throw RelayStateException("key distribution message for relay has already been processed");
    if (relay->holders.key_quarter_holders.size() == 0)
        AssignKeyPartHoldersToRelay(*relay, key_distribution_message.encoding_message_hash);
    relay->hashes.key_distribution_message_hash = key_distribution_message.GetHash160();
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
    uint160 random_seed = relay->GetSeedForDeterminingSuccessor();
    Relay *chosen_successor{NULL};
    bool found = false;

    while (not found)
    {
        random_seed = HashUint160(random_seed);
        chosen_successor = &relays[(CBigNum(random_seed) % CBigNum(relays.size())).getulong()];
        if (SuccessorToRelayIsSuitable(chosen_successor, relay))
            found = true;
    }
    return chosen_successor->number;
}

bool RelayState::SuccessorToRelayIsSuitable(Relay *chosen_successor, Relay *relay)
{
    if (chosen_successor->number == relay->number)
        return false;
    if (VectorContainsEntry(relay->holders.key_quarter_holders, chosen_successor->number))
        return false;
    std::vector<uint64_t> key_quarter_sharers_of_relay = RelayNumbersOfRelaysWhoseKeyQuartersAreHeldByGivenRelay(relay);
    if (VectorContainsEntry(key_quarter_sharers_of_relay, chosen_successor->number))
        return false;
    for (auto key_quarter_sharer_number : key_quarter_sharers_of_relay)
        if (VectorContainsEntry(chosen_successor->holders.key_quarter_holders, key_quarter_sharer_number))
            return false;
    return true;
}

std::vector<uint64_t> RelayState::RelayNumbersOfRelaysWhoseKeyQuartersAreHeldByGivenRelay(Relay *given_relay)
{
    std::vector<uint64_t> relays_with_key_quarters_held;

    for (Relay &relay : relays)
        if (VectorContainsEntry(relay.holders.key_quarter_holders, given_relay->number))
            relays_with_key_quarters_held.push_back(relay.number);

    return relays_with_key_quarters_held;
}

Obituary RelayState::GenerateObituary(Relay *relay, uint32_t reason_for_leaving)
{
    Obituary obituary;
    obituary.dead_relay_number = relay->number;
    obituary.reason_for_leaving = reason_for_leaving;
    obituary.relay_state_hash = GetHash160();
    obituary.in_good_standing =
            reason_for_leaving == SAID_GOODBYE and RelayIsOldEnoughToLeaveInGoodStanding(relay);
    obituary.successor_number = AssignSuccessorToRelay(relay);
    return obituary;
}

bool RelayState::RelayIsOldEnoughToLeaveInGoodStanding(Relay *relay)
{
    uint64_t latest_relay_number = relays.back().number;
    return latest_relay_number > relay->number + 1440;
}

void RelayState::ProcessObituary(Obituary obituary, Data data)
{
    Relay *relay = GetRelayByNumber(obituary.dead_relay_number);
    if (relay == NULL) throw RelayStateException("no such relay");
    relay->hashes.obituary_hash = obituary.GetHash160();

    if (obituary.reason_for_leaving != SAID_GOODBYE)
        for (auto key_quarter_holder_relay_number : relay->holders.key_quarter_holders)
        {
            Relay *key_quarter_holder = GetRelayByNumber(key_quarter_holder_relay_number);
            key_quarter_holder->tasks.push_back(relay->hashes.obituary_hash);
        }
}

RelayExit RelayState::GenerateRelayExit(Relay *relay)
{
    RelayExit relay_exit;
    relay_exit.obituary_hash = relay->hashes.obituary_hash;
    relay_exit.secret_recovery_message_hashes = relay->hashes.secret_recovery_message_hashes;
    return relay_exit;
}

void RelayState::ProcessRelayExit(RelayExit relay_exit, Data data)
{
    Obituary obituary = data.msgdata[relay_exit.obituary_hash]["obituary"];
    if (not data.msgdata[relay_exit.obituary_hash].HasProperty("obituary"))
        throw RelayStateException("no record of obituary specified in relay exit");
    if (GetRelayByNumber(obituary.dead_relay_number) == NULL)
        throw RelayStateException("no relay with specified number to remove");
    TransferTasksToSuccessor(obituary.dead_relay_number, obituary.successor_number);
    RemoveRelay(obituary.dead_relay_number, obituary.successor_number);
}

void RelayState::TransferTasksToSuccessor(uint64_t dead_relay_number, uint64_t successor_number)
{
    Relay *successor = GetRelayByNumber(successor_number);
    Relay *dead_relay = GetRelayByNumber(dead_relay_number);
    if (dead_relay == NULL or successor == NULL) throw RelayStateException("no such relay");

    successor->tasks = ConcatenateVectors(successor->tasks, dead_relay->tasks);
}

void RelayState::RemoveRelay(uint64_t exiting_relay_number, uint64_t successor_relay_number)
{
    for (uint32_t i = 0; i < relays.size(); i++)
    {
        if (relays[i].number == exiting_relay_number and remove_dead_relays)
            relays.erase(relays.begin() + i);
        for (uint32_t j = 0; j < relays[i].holders.key_quarter_holders.size(); j++)
            if (relays[i].holders.key_quarter_holders[j] == exiting_relay_number)
                relays[i].holders.key_quarter_holders[j] = successor_relay_number;
    }
    maps_are_up_to_date = false;
}

void RelayState::ProcessKeyDistributionComplaint(KeyDistributionComplaint complaint, Data data)
{
    Relay *secret_sender = complaint.GetSecretSender(data);
    if (secret_sender == NULL) throw RelayStateException("no such relay");

    if (secret_sender->key_distribution_message_accepted)
        throw RelayStateException("too late to process complaint");

    secret_sender->hashes.key_distribution_complaint_hashes.push_back(complaint.GetHash160());

    RecordRelayDeath(secret_sender, data, MISBEHAVED);
}

void RelayState::ProcessDurationWithoutResponse(DurationWithoutResponse duration, Data data)
{
    std::string message_type = data.msgdata[duration.message_hash]["type"];

    if (message_type == "key_distribution")
        ProcessDurationAfterKeyDistributionMessage(data.msgdata[duration.message_hash][message_type], data);

    else if (message_type == "goodbye")
        ProcessDurationAfterGoodbyeMessage(data.msgdata[duration.message_hash][message_type], data);

    else if (message_type == "secret_recovery")
        ProcessDurationAfterSecretRecoveryMessage(data.msgdata[duration.message_hash][message_type], data);
}

void RelayState::ProcessDurationAfterKeyDistributionMessage(KeyDistributionMessage key_distribution_message, Data data)
{
    Relay *relay = GetRelayByJoinHash(key_distribution_message.relay_join_hash);
    if (relay == NULL or relay->hashes.obituary_hash != 0) throw RelayStateException("non-existent or dead relay");
    relay->key_distribution_message_accepted = true;
}

void RelayState::ProcessGoodbyeMessage(GoodbyeMessage goodbye)
{
    Relay *relay = GetRelayByNumber(goodbye.dead_relay_number);
    if (relay == NULL) throw RelayStateException("no such relay");
    relay->hashes.goodbye_message_hash = goodbye.GetHash160();
}

void RelayState::ProcessGoodbyeComplaint(GoodbyeComplaint complaint, Data data)
{
    Relay *dead_relay = complaint.GetSecretSender(data);

    if (dead_relay == NULL) throw RelayStateException("no such relay");
    dead_relay->hashes.goodbye_complaint_hashes.push_back(complaint.GetHash160());
    RecordRelayDeath(dead_relay, data, MISBEHAVED);
}

void RelayState::ProcessDurationAfterGoodbyeMessage(GoodbyeMessage goodbye, Data data)
{
    Relay *successor = data.relay_state->GetRelayByNumber(goodbye.successor_number);
    if (successor == NULL) throw RelayStateException("no such relay");
    RecordRelayDeath(successor, data, NOT_RESPONDING);
}

void RelayState::ProcessSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message)
{
    Relay *dead_relay = GetRelayByNumber(secret_recovery_message.dead_relay_number);
    Relay *quarter_holder = GetRelayByNumber(secret_recovery_message.quarter_holder_number);
    Relay *successor = GetRelayByNumber(secret_recovery_message.successor_number);
    if (dead_relay == NULL  or quarter_holder == NULL or successor == NULL)
        throw RelayStateException("secret recovery message refers to non-existent relay");

    EraseEntryFromVector(secret_recovery_message.obituary_hash, quarter_holder->tasks);
    dead_relay->hashes.secret_recovery_message_hashes.push_back(secret_recovery_message.GetHash160());
    if (dead_relay->hashes.secret_recovery_message_hashes.size() == 4)
        successor->tasks.push_back(secret_recovery_message.GetHash160());
}

void RelayState::ProcessDurationAfterSecretRecoveryMessage(SecretRecoveryMessage secret_recovery_message, Data data)
{
    Relay *successor = secret_recovery_message.GetSuccessor(data);
    if (successor == NULL)
        throw RelayStateException("secret recovery message refers to non-existent successor");
    RecordRelayDeath(successor, data, NOT_RESPONDING);
}

void RelayState::ProcessSecretRecoveryComplaint(SecretRecoveryComplaint complaint, Data data)
{
    Relay *key_quarter_holder = complaint.GetSecretSender(data);
    Relay *dead_relay = complaint.GetDeadRelay(data);
    if (key_quarter_holder == NULL or dead_relay == NULL)
        throw RelayStateException("secret recovery complaint refers to non-existent relay");

    auto recovery_message = complaint.GetSecretRecoveryMessage(data);

    key_quarter_holder->tasks.push_back(recovery_message.obituary_hash);
    RecordRelayDeath(key_quarter_holder, data, MISBEHAVED);
    dead_relay->hashes.secret_recovery_complaint_hashes.push_back(complaint.GetHash160());
    EraseEntryFromVector(complaint.secret_recovery_message_hash, dead_relay->hashes.secret_recovery_message_hashes);
}

void RelayState::RecordRelayDeath(Relay *dead_relay, Data data, uint32_t reason)
{
    if (dead_relay->hashes.obituary_hash != 0)
        return;
    Obituary obituary = GenerateObituary(dead_relay, reason);
    data.StoreMessage(obituary);
    ProcessObituary(obituary, data);
}

void RelayState::ProcessDurationWithoutResponseFromRelay(DurationWithoutResponseFromRelay duration, Data data)
{
    std::string message_type = data.msgdata[duration.message_hash]["type"];

    if (message_type == "obituary")
    {
        Obituary obituary = data.msgdata[duration.message_hash][message_type];
        ProcessDurationWithoutRelayResponseAfterObituary(obituary, duration.relay_number, data);
    }

    else if (message_type == "secret_recovery_failure")
    {
        SecretRecoveryFailureMessage failure_message = data.msgdata[duration.message_hash][message_type];
        ProcessDurationWithoutRelayResponseAfterSecretRecoveryFailureMessage(failure_message,
                                                                             duration.relay_number, data);
    }
}

void RelayState::ProcessDurationWithoutRelayResponseAfterObituary(Obituary obituary, uint64_t relay_number, Data data)
{
    Relay *key_quarter_holder = GetRelayByNumber(relay_number);
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
    dead_relay->hashes.recovery_failure_audit_message_hashes.push_back(audit_message.GetHash160());
    Relay *key_quarter_holder = GetRelayByNumber(audit_message.quarter_holder_number);
    EraseEntryFromVector(audit_message.failure_message_hash, key_quarter_holder->tasks);

    ExamineAuditMessageToDetermineWrongdoer(audit_message, dead_relay, key_quarter_holder, data);
}

void RelayState::ExamineAuditMessageToDetermineWrongdoer(RecoveryFailureAuditMessage audit_message,
                                                         Relay *dead_relay, Relay *quarter_holder, Data data)
{
    if (not audit_message.ContainsCorrectData(data))
    {
        RecordRelayDeath(quarter_holder, data, MISBEHAVED);
        data.msgdata[audit_message.failure_message_hash]["bad_quarter_holder_found"] = true;
        return;
    }
    auto audit_messages = dead_relay->hashes.recovery_failure_audit_message_hashes;
    if (audit_messages.size() == 4 and not data.msgdata[audit_message.failure_message_hash]["bad_quarter_holder_found"])
        DetermineWhetherSuccessorOrKeySharerIsAtFaultInSecretRecoveryFailure(dead_relay,
                                                                             audit_message.failure_message_hash, data);
}

void RelayState::DetermineWhetherSuccessorOrKeySharerIsAtFaultInSecretRecoveryFailure(Relay *dead_relay,
                                                                                      uint160 failure_message_hash,
                                                                                      Data data)
{
    if (SuccessorWasLyingAboutSumOfRecoveredSharedSecretQuarters(dead_relay, failure_message_hash, data))
    {
        auto successor = data.relay_state->GetRelayByNumber(dead_relay->SuccessorNumber(data));
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
    auto public_key_sixteenth = failure_message.GetKeySixteenth(data);
    Point sum_of_shared_secret_quarters(CBigNum(0));
    for (auto audit_message_hash : dead_relay->hashes.recovery_failure_audit_message_hashes)
    {
        RecoveryFailureAuditMessage audit_message = data.GetMessage(audit_message_hash);
        sum_of_shared_secret_quarters += audit_message.private_receiving_key_quarter * public_key_sixteenth;
    }
    return sum_of_shared_secret_quarters != failure_message.sum_of_decrypted_shared_secret_quarters;
}

void RelayState::ProcessSecretRecoveryFailureMessage(SecretRecoveryFailureMessage failure_message, Data data)
{
    auto dead_relay = failure_message.GetDeadRelay(data);
    auto quarter_holders = failure_message.GetQuarterHolders(data);
    uint160 failure_message_hash = failure_message.GetHash160();
    dead_relay->hashes.secret_recovery_failure_message_hashes.push_back(failure_message_hash);

    for (auto quarter_holder : quarter_holders)
        if (not VectorContainsEntry(quarter_holder->tasks, failure_message_hash))
            quarter_holder->tasks.push_back(failure_message_hash);
}

bool RelayState::ContainsRelayWithNumber(uint64_t relay_number)
{
    return GetRelayByNumber(relay_number) != NULL;
}

void RelayState::ProcessSuccessionCompletedMessage(SuccessionCompletedMessage &succession_completed_message, Data data)
{
    auto dead_relay = GetRelayByNumber(succession_completed_message.dead_relay_number);
    auto successor = GetRelayByNumber(succession_completed_message.successor_number);
    successor->hashes.succession_completed_message_hashes.push_back(succession_completed_message.GetHash160());
    if (succession_completed_message.recovery_message_hashes.size() == 0)
        RecordRelayDeath(dead_relay, data, SAID_GOODBYE);
    ProcessRelayExit(GenerateRelayExit(dead_relay), data);
}
