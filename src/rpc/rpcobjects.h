#ifndef FLEX_RPCOBJECTS
#define FLEX_RPCOBJECTS

#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_reader.h"
#include "json/json_spirit.h"

#include "flexnode/calendar.h"
#include "trade/trade_messages.h"
#include "relays/relaystate.h"
#include "relays/relay_messages.h"


json_spirit::Object GetObjectFromTrade(uint160 accept_commit_hash);

json_spirit::Object GetObjectFromOrder(Order order);

std::string OrderStatus(uint160 order_hash);

json_spirit::Object GetObjectFromMinedCredit(MinedCredit credit);

json_spirit::Object GetObjectFromCalendar(Calendar calendar);

json_spirit::Object GetObjectFromCredit(Credit credit);

json_spirit::Object GetObjectFromCreditInBatch(CreditInBatch credit);

json_spirit::Object GetObjectFromBatch(CreditBatch batch);

json_spirit::Object GetObjectFromMinedCreditMessage(MinedCreditMessage& msg);

json_spirit::Object GetObjectFromTransaction(SignedTransaction tx);

json_spirit::Object GetObjectFromRelayState(RelayState state);

json_spirit::Object GetObjectFromJoin(RelayJoinMessage join);

json_spirit::Object GetObjectFromSuccession(SuccessionMessage succession);

json_spirit::Object GetObjectFromDepositAddressRequest(uint160 request_hash);

#endif