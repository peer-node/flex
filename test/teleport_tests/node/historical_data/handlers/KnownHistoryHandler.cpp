#include <test/teleport_tests/node/historical_data/messages/DiurnDataRequest.h>
#include <test/teleport_tests/node/historical_data/handlers/CalendarHandler.h>
#include <test/teleport_tests/node/credit/handlers/CreditMessageHandler.h>
#include <test/teleport_tests/node/TeleportNetworkNode.h>
#include "KnownHistoryHandler.h"

#include "log.h"
#define LOG_CATEGORY "KnownHistoryHandler.cpp"

KnownHistoryMessage KnownHistoryHandler::GenerateKnownHistoryMessage()
{
    KnownHistoryRequest request(data_message_handler->calendar->LastMinedCreditMessageHash());
    KnownHistoryMessage known_history(request, credit_system);
    return known_history;
}

uint64_t NumberOfBitsSet(BitChain& bit_chain)
{
    uint64_t bits_set = 0;
    for (uint64_t i = 0; i < bit_chain.length; i++)
        if (bit_chain.Get(i))
            bits_set++;
    return bits_set;
}

bool KnownHistoryHandler::ValidateKnownHistoryMessage(KnownHistoryMessage known_history_message)
{
    uint64_t diurns_known = NumberOfBitsSet(known_history_message.history_declaration.known_diurns);

    if (diurns_known != known_history_message.history_declaration.diurn_hashes.size())
        return false;

    uint160 msg_hash = known_history_message.mined_credit_message_hash;

    uint64_t number_of_calends = Calendar(msg_hash, credit_system).calends.size();

    return number_of_calends == known_history_message.history_declaration.known_diurns.length;
}

void KnownHistoryHandler::HandleKnownHistoryMessage(KnownHistoryMessage known_history)
{
    if (not msgdata[known_history.request_hash]["is_history_request"] or not ValidateKnownHistoryMessage(known_history))
    {
        data_message_handler->RejectMessage(known_history.GetHash160());
        return;
    }
    RecordDiurnsWhichPeerKnows(known_history);
}

void KnownHistoryHandler::RecordDiurnsWhichPeerKnows(KnownHistoryMessage known_history)
{
    CNode *peer = data_message_handler->GetPeer(known_history.GetHash160());
    if (peer == NULL)
        return;
    int peer_id = peer->id;
    Calendar calendar_(known_history.mined_credit_message_hash, credit_system);
    for (uint32_t i = 0; i < calendar_.calends.size(); i++)
    {
        if (known_history.history_declaration.known_diurns.Get(i))
        {
            std::vector<int> peers_with_data = msgdata[calendar_.calends[i].DiurnRoot()]["peers_with_data"];
            if (not VectorContainsEntry(peers_with_data, peer_id))
                peers_with_data.push_back(peer_id);
            msgdata[calendar_.calends[i].DiurnRoot()]["peers_with_data"] = peers_with_data;
        }
    }
}

void KnownHistoryHandler::HandleKnownHistoryRequest(KnownHistoryRequest request)
{
    KnownHistoryMessage history_message(request, credit_system);
    CNode *peer = data_message_handler->GetPeer(request.GetHash160());
    if (peer != NULL)
        peer->PushMessage("data", "known_history", history_message);
}

uint160 KnownHistoryHandler::RequestKnownHistoryMessages(uint160 mined_credit_message_hash)
{
    KnownHistoryRequest request(mined_credit_message_hash);
    uint160 request_hash = request.GetHash160();

    msgdata[request_hash]["is_history_request"] = true;
    msgdata[request_hash]["known_history_request"] = request;

    data_message_handler->Broadcast(request);
    return request_hash;
}

uint160 KnownHistoryHandler::RequestDiurnData(KnownHistoryMessage history_message, std::vector<uint32_t> requested_diurns, CNode *peer)
{
    if (peer == NULL)
        return 0;
    DiurnDataRequest request(history_message, requested_diurns);
    uint160 request_hash = request.GetHash160();

    msgdata[request_hash]["is_diurn_data_request"] = true;
    msgdata[request_hash]["diurn_data_request"] = request;

    peer->PushMessage("data", "diurn_data_request", request);
    return request_hash;
}

void KnownHistoryHandler::HandleDiurnDataMessage(DiurnDataMessage diurn_data_message)
{
    if (not msgdata[diurn_data_message.request_hash]["is_diurn_data_request"])
    {
        log_ << "unrequested diurn data message\n";
        data_message_handler->RejectMessage(diurn_data_message.GetHash160());
        return;
    }
    if (not ValidateDataInDiurnDataMessage(diurn_data_message))
    {
        log_ << "failed to validate data in diurn data message\n";
        data_message_handler->RejectMessage(diurn_data_message.GetHash160());
        return;
    }
    credit_system->StoreDataFromDiurnDataMessage(diurn_data_message);
    ScrutinizeDiurnsAndSendAFailureMessageIfABadBatchIsFound(diurn_data_message);
}

void KnownHistoryHandler::ScrutinizeDiurnsAndSendAFailureMessageIfABadBatchIsFound(DiurnDataMessage diurn_data_message)
{
    DiurnFailureDetails details;
    uint32_t bad_diurn;
    bool ok = CheckForFailuresInProofsOfWorkInDiurnDataMessage(diurn_data_message,
                                                               details, bad_diurn, diurn_scrutiny_time);
    if (not ok)
    {
        log_ << "sending diurn failure message\n";
        DiurnFailureMessage diurn_failure_message;
        diurn_failure_message.details = details;
        diurn_failure_message.diurn = diurn_data_message.diurns[bad_diurn];
        msgdata[diurn_data_message.GetHash160()]["failure_message_hash"] = diurn_failure_message.GetHash160();
        msgdata[diurn_failure_message.GetHash160()]["diurn_failure"] = diurn_failure_message;
        data_message_handler->Broadcast(diurn_failure_message);
        HandleDiurnFailureMessage(diurn_failure_message);
    }
}

void KnownHistoryHandler::HandlerDiurnDataRequest(DiurnDataRequest request)
{
    CNode *peer = data_message_handler->GetPeer(request.GetHash160());
    if (peer == NULL)
        return;
    DiurnDataMessage diurn_data_message(request, credit_system);
    peer->PushMessage("data", "diurn_data", diurn_data_message);
}

bool KnownHistoryHandler::CheckIfDiurnDataMatchesHashes(DiurnDataMessage diurn_data,
                                                        KnownHistoryMessage history_message,
                                                        DiurnDataRequest diurn_data_request)
{
    KnownHistoryDeclaration &declaration = history_message.history_declaration;
    for (uint64_t i = 0; i < diurn_data_request.requested_diurns.size(); i++)
    {
        uint64_t diurn_index = diurn_data_request.requested_diurns[i];
        if (diurn_data.diurns[i].GetHash160() != declaration.diurn_hashes[diurn_index])
        {
            log_ << "diurn hash mismatch\n";
            return false;
        }
        if (diurn_data.message_data[i].GetHash160() != declaration.diurn_message_data_hashes[diurn_index])
        {
            log_ << "diurn message data hash mismatch\n";
            return false;
        }
    }
    return true;
}

bool KnownHistoryHandler::ValidateDataInDiurnDataMessage(DiurnDataMessage diurn_data_message)
{
    MemoryDataStore msgdata_, creditdata_, keydata_, depositdata_;
    Data data_(msgdata_, creditdata_, keydata_, depositdata_);
    CreditSystem credit_system_(data_);

    if (not CheckSizesInDiurnDataMessage(diurn_data_message))
    {
        log_ << "wrong sizes\n";
        return false;
    }

    DiurnDataRequest request = msgdata[diurn_data_message.request_hash]["diurn_data_request"];
    KnownHistoryMessage known_history_message = msgdata[request.known_history_message_hash]["known_history"];

    if (not CheckIfDiurnDataMatchesHashes(diurn_data_message, known_history_message, request))
    {
        log_ << "failed hashes\n";
        return false;
    }

    credit_system_.StoreDataFromDiurnDataMessage(diurn_data_message);

    credit_system_.SetMiningParameters(data_message_handler->calendar_handler->number_of_megabytes_for_mining,
                                       credit_system->initial_difficulty,
                                       credit_system->initial_diurnal_difficulty);

    for (uint32_t i = 0; i < diurn_data_message.diurns.size(); i++)
    {
        if (not ValidateDataInDiurn(diurn_data_message.diurns[i], &credit_system_,
                                    diurn_data_message.initial_spent_chains[i]))
            return false;
    }
    return true;
}

bool KnownHistoryHandler::ValidateDataInDiurn(Diurn &diurn, CreditSystem *credit_system_, BitChain &initial_spent_chain)
{
    MemoryDataStore keydata_, depositdata_;
    Data data_(credit_system_->msgdata, credit_system_->creditdata, keydata_, depositdata_);
    CreditMessageHandler credit_message_handler(data_);
    credit_message_handler.SetCreditSystem(credit_system_);
    credit_message_handler.do_spot_checks = false;

    credit_message_handler.SetSpentChain(initial_spent_chain);

    Calendar calendar_(diurn.credits_in_diurn[0].GetHash160(), credit_system_);
    TrimLastDiurnFromCalendar(calendar_, credit_system_);
    credit_message_handler.SetCalendar(calendar_);

    for (auto mined_credit_message : diurn.credits_in_diurn)
        credit_message_handler.Handle(mined_credit_message, NULL);

    if (calendar_.LastMinedCreditMessageHash() != diurn.credits_in_diurn.back().GetHash160())
    {
        log_ << "failed at " << calendar_.LastMinedCreditMessageHash() << "\n";
        log_ << "diurn size is " << diurn.Size() << "\n";
        log_ << "calendar diurn size is " << calendar_.current_diurn.Size() << "\n";
        return false;
    }
    return true;
}

void KnownHistoryHandler::TrimLastDiurnFromCalendar(Calendar& calendar, CreditSystem *credit_system)
{
    if (calendar.current_diurn.Size() == 1 and calendar.calends.size() > 0)
    {
        calendar.RemoveLast(credit_system);
    }

    while (calendar.current_diurn.Size() > 1)
        calendar.RemoveLast(credit_system);
}

bool KnownHistoryHandler::CheckSizesInDiurnDataMessage(DiurnDataMessage diurn_data_message)
{
    uint64_t number_of_diurns = diurn_data_message.diurns.size();
    return diurn_data_message.message_data.size() == number_of_diurns and
           diurn_data_message.initial_spent_chains.size() == number_of_diurns and
           diurn_data_message.calends.size() == number_of_diurns;
}

bool KnownHistoryHandler::CheckForFailuresInProofsOfWorkInDiurnDataMessage(DiurnDataMessage &diurn_data_message,
                                                                           DiurnFailureDetails &failure_details,
                                                                           uint32_t &bad_diurn,
                                                                           uint64_t scrutiny_time)
{
    uint64_t start_time = GetTimeMicros();
    auto diurns = diurn_data_message.diurns;

    while (GetTimeMicros() - start_time < scrutiny_time)
    {
        uint32_t selected_diurn = (uint32_t) GetRand(diurns.size());
        uint32_t selected_msg = (uint32_t) GetRand(diurns[selected_diurn].Size());
        bool ok = CheckForFailuresInMinedCreditMessageFromDiurnDataMessage(diurn_data_message, failure_details,
                                                                           bad_diurn, selected_diurn, selected_msg);
        if (not ok)
            return false;
    }
    return true;
}

bool KnownHistoryHandler::CheckForFailuresInMinedCreditMessageFromDiurnDataMessage(DiurnDataMessage &diurn_data_message,
                                                                                   DiurnFailureDetails &failure_details,
                                                                                   uint32_t &bad_diurn,
                                                                                   uint32_t diurn_to_check,
                                                                                   uint32_t msg_to_check)
{
    Diurn &diurn = diurn_data_message.diurns[diurn_to_check];
    MinedCreditMessage &msg = diurn.credits_in_diurn[msg_to_check];
    auto check = msg.proof_of_work.proof.SpotCheck();
    if (not check.valid)
    {
        bad_diurn = diurn_to_check;
        failure_details.check = check;
        failure_details.mined_credit_message_hash = msg.GetHash160();
        failure_details.preceding_calend_hash = diurn.credits_in_diurn.front().GetHash160();
        failure_details.succeeding_calend_hash = diurn_data_message.calends[diurn_to_check].GetHash160();
        return false;
    }
    return true;
}

bool KnownHistoryHandler::CheckHashesInDiurnDataMessage(DiurnDataMessage diurn_data_message)
{
    for (uint64_t diurn_number = 0; diurn_number < diurn_data_message.diurns.size(); diurn_number++)
    {
        Diurn &diurn = diurn_data_message.diurns[diurn_number];
        Calend &calend = diurn_data_message.calends[diurn_number];
        if (calend.DiurnRoot() != diurn.Root())
            return false;

        uint160 hash_of_msg_preceding_calend = calend.mined_credit.network_state.previous_mined_credit_message_hash;
        if (hash_of_msg_preceding_calend != diurn.credits_in_diurn.back().GetHash160())
            return false;

        uint160 hash_of_calend_preceding_calend = calend.mined_credit.network_state.previous_calend_hash;
        if (hash_of_calend_preceding_calend != diurn.credits_in_diurn.front().GetHash160())
            return false;

        if (not CheckHashesInDiurn(diurn))
            return false;
    }
    return true;
}

bool KnownHistoryHandler::CheckHashesInDiurn(Diurn &diurn)
{
    uint160 previous_msg_hash = 0;
    for (auto msg : diurn.credits_in_diurn)
    {
        if (previous_msg_hash != 0 and
            msg.mined_credit.network_state.previous_mined_credit_message_hash != previous_msg_hash)
            return false;
        previous_msg_hash = msg.GetHash160();
    }
    return true;
}

void KnownHistoryHandler::HandleDiurnFailureMessage(DiurnFailureMessage diurn_failure_message)
{
    if (not ValidateDiurnFailureMessage(diurn_failure_message))
    {
        data_message_handler->RejectMessage(diurn_failure_message.GetHash160());
        return;
    }
    credit_system->RemoveMinedCreditMessagesDownstreamOfDiurnFailure(diurn_failure_message);
    if (data_message_handler->teleport_network_node != NULL and
            data_message_handler->teleport_network_node->credit_message_handler != NULL)
        data_message_handler->teleport_network_node->tip_controller.SwitchToNewTipIfAppropriate();
}

bool KnownHistoryHandler::ValidateDiurnFailureMessage(DiurnFailureMessage diurn_failure_message)
{
    Calend calend_succeeding_diurn = msgdata[diurn_failure_message.details.succeeding_calend_hash]["msg"];
    Diurn &diurn = diurn_failure_message.diurn;
    if (calend_succeeding_diurn.DiurnRoot() != diurn.Root())
    {
        log_ << "bad diurn root\n";
        return false;
    }
    if (not CheckHashesInDiurn(diurn))
    {
        log_ << "bad hashes in diurn\n";
        return false;
    }
    if (diurn.credits_in_diurn[0].GetHash160() != diurn_failure_message.details.preceding_calend_hash)
    {
        log_ << "preceding calend doesn't match\n";
        return false;
    }
    if (not VerifyFailureInDiurnFailureMessage(diurn_failure_message))
    {
        log_ << "report of failure is false\n";
        return false;
    }
    return true;
}

bool KnownHistoryHandler::VerifyFailureInDiurnFailureMessage(DiurnFailureMessage diurn_failure_message)
{
    MinedCreditMessage bad_message;
    for (auto msg : diurn_failure_message.diurn.credits_in_diurn)
    {
        if (msg.GetHash160() == diurn_failure_message.details.mined_credit_message_hash)
            bad_message = msg;
    }
    if (bad_message.mined_credit.network_state.batch_number == 0)
        return false;

    TwistWorkProof &proof = bad_message.proof_of_work.proof;
    return (bool) diurn_failure_message.details.check.VerifyInvalid(proof);
}
