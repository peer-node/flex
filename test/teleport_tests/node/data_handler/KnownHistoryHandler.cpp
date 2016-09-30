#include <test/teleport_tests/node/data_handler/DiurnDataRequest.h>
#include "KnownHistoryHandler.h"


KnownHistoryMessage KnownHistoryHandler::GenerateKnownHistoryMessage()
{
    KnownHistoryMessage known_history(data_message_handler->calendar->LastMinedCreditMessageHash(),
                                      data_message_handler->credit_system);
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

    uint64_t data_known = NumberOfBitsSet(known_history_message.history_declaration.known_diurn_message_data);

    if (data_known != known_history_message.history_declaration.diurn_message_data_hashes.size())
        return false;

    uint160 msg_hash = known_history_message.mined_credit_message_hash;

    uint64_t number_of_calends = Calendar(msg_hash, data_message_handler->credit_system).calends.size();

    return number_of_calends == known_history_message.history_declaration.known_diurns.length and
        number_of_calends == known_history_message.history_declaration.known_diurn_message_data.length;
}

void KnownHistoryHandler::HandleKnownHistoryMessage(KnownHistoryMessage known_history)
{
    if (not msgdata[known_history.request_hash]["is_history_request"])
    {
        data_message_handler->RejectMessage(known_history.GetHash160());
        return;
    }
}

void KnownHistoryHandler::HandleKnownHistoryRequest(KnownHistoryRequest request)
{
    KnownHistoryMessage history_message(request, data_message_handler->credit_system);
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

uint160 KnownHistoryHandler::RequestDiurnData(uint160 msg_hash, std::vector<uint32_t> requested_diurns, CNode *peer)
{
    if (peer == NULL)
        return 0;
    DiurnDataRequest request(msg_hash, requested_diurns);
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
        data_message_handler->RejectMessage(diurn_data_message.GetHash160());
        return;
    }
}

void KnownHistoryHandler::HandlerDiurnDataRequest(DiurnDataRequest request)
{
    CNode *peer = data_message_handler->GetPeer(request.GetHash160());
    if (peer == NULL)
        return;
    DiurnDataMessage diurn_data_message(request, data_message_handler->credit_system);
    peer->PushMessage("data", "diurn_data", diurn_data_message);
}
