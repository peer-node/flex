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

    uint160 msg_hash = known_history_message.latest_mined_credit_message_hash;

    uint64_t number_of_calends = Calendar(msg_hash, data_message_handler->credit_system).calends.size();

    if (number_of_calends != known_history_message.history_declaration.known_diurns.length or
        number_of_calends != known_history_message.history_declaration.known_diurn_message_data.length)
        return false;

    return true;
}
