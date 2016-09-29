#include "DataMessageHandler.h"
#include "CalendarFailureMessage.h"
#include "test/flex_tests/node/CreditMessageHandler.h"
#include "test/flex_tests/node/FlexNetworkNode.h"
#include "TipHandler.h"
#include "CalendarHandler.h"
#include "InitialDataHandler.h"
#include "KnownHistoryHandler.h"

#include "log.h"
#define LOG_CATEGORY "DataMessageHandler.cpp"

using std::vector;



DataMessageHandler::DataMessageHandler(MemoryDataStore &msgdata_, MemoryDataStore &creditdata_):
    MessageHandlerWithOrphanage(msgdata_), msgdata(msgdata_), creditdata(creditdata_)
{
    channel = std::string("data");
}

DataMessageHandler::~DataMessageHandler()
{
    if (tip_handler != NULL)
        delete tip_handler;

    if (calendar_handler != NULL)
        delete calendar_handler;

    if (initial_data_handler != NULL)
        delete initial_data_handler;

    if (known_history_handler != NULL)
        delete known_history_handler;
}

void DataMessageHandler::SetCreditSystemAndGenerateHandlers(CreditSystem *credit_system_)
{
    credit_system = credit_system_;

    tip_handler = new TipHandler(this);
    calendar_handler = new CalendarHandler(this);
    initial_data_handler = new InitialDataHandler(this);
    known_history_handler = new KnownHistoryHandler(this);
}

void DataMessageHandler::SetCalendar(Calendar *calendar_)
{
    calendar = calendar_;
}

void DataMessageHandler::HandleTipRequestMessage(TipRequestMessage request)
{
    tip_handler->HandleTipRequestMessage(request);
}

void DataMessageHandler::HandleTipMessage(TipMessage tip_message)
{
    tip_handler->HandleTipMessage(tip_message);
}

void DataMessageHandler::HandleCalendarRequestMessage(CalendarRequestMessage request)
{
    calendar_handler->HandleCalendarRequestMessage(request);
}

void DataMessageHandler::HandleCalendarMessage(CalendarMessage calendar_message)
{
    calendar_handler->HandleCalendarMessage(calendar_message);
}

void DataMessageHandler::HandleInitialDataRequestMessage(InitialDataRequestMessage request)
{
    initial_data_handler->HandleInitialDataRequestMessage(request);
}

void DataMessageHandler::HandleInitialDataMessage(InitialDataMessage initial_data_message)
{
    initial_data_handler->HandleInitialDataMessage(initial_data_message);
}

void DataMessageHandler::SetNetworkNode(FlexNetworkNode *flex_network_node_)
{
    flex_network_node = flex_network_node_;
}

void DataMessageHandler::HandleCalendarFailureMessage(CalendarFailureMessage message)
{
    calendar_handler->HandleCalendarFailureMessage(message);
}

void DataMessageHandler::HandleKnownHistoryMessage(KnownHistoryMessage known_history_message)
{
    known_history_handler->HandleKnownHistoryMessage(known_history_message);
}

void DataMessageHandler::HandleKnownHistoryRequest(KnownHistoryRequest known_history_request)
{
    known_history_handler->HandleKnownHistoryRequest(known_history_request);
}
