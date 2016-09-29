#ifndef FLEX_DATAMESSAGEHANDLER_H
#define FLEX_DATAMESSAGEHANDLER_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "test/flex_tests/node/MessageHandlerWithOrphanage.h"
#include "test/flex_tests/node/TipRequestMessage.h"
#include "test/flex_tests/node/CreditSystem.h"
#include "test/flex_tests/node/Calendar.h"
#include "test/flex_tests/node/TipMessage.h"
#include "CalendarRequestMessage.h"
#include "CalendarMessage.h"
#include "CalendarFailureDetails.h"
#include "InitialDataRequestMessage.h"
#include "InitialDataMessage.h"
#include "CalendarFailureMessage.h"
#include "KnownHistoryMessage.h"


#define CALENDAR_SCRUTINY_TIME 5000000 // microseconds

class FlexNetworkNode;

class TipHandler;
class CalendarHandler;
class InitialDataHandler;
class KnownHistoryHandler;


class DataMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &msgdata, &creditdata;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    FlexNetworkNode *flex_network_node{NULL};

    TipHandler *tip_handler{NULL};
    CalendarHandler *calendar_handler{NULL};
    InitialDataHandler *initial_data_handler{NULL};
    KnownHistoryHandler *known_history_handler{NULL};

    DataMessageHandler(MemoryDataStore &msgdata_, MemoryDataStore &creditdata_);

    ~DataMessageHandler();

    void SetCreditSystemAndGenerateHandlers(CreditSystem *credit_system_);

    void SetCalendar(Calendar *calendar_);

    void SetNetworkNode(FlexNetworkNode *flex_network_node_);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(TipRequestMessage);
        HANDLESTREAM(TipMessage);
        HANDLESTREAM(CalendarRequestMessage);
        HANDLESTREAM(CalendarMessage);
        HANDLESTREAM(CalendarFailureMessage);
        HANDLESTREAM(InitialDataRequestMessage);
        HANDLESTREAM(InitialDataMessage);
        HANDLESTREAM(KnownHistoryRequest);
        HANDLESTREAM(KnownHistoryMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(TipRequestMessage);
        HANDLEHASH(TipMessage);
        HANDLEHASH(CalendarRequestMessage);
        HANDLEHASH(CalendarMessage);
        HANDLEHASH(CalendarFailureMessage);
        HANDLEHASH(InitialDataRequestMessage);
        HANDLEHASH(InitialDataMessage);
        HANDLEHASH(KnownHistoryMessage);
        HANDLEHASH(KnownHistoryRequest);
    }

    HANDLECLASS(TipRequestMessage);
    HANDLECLASS(TipMessage);
    HANDLECLASS(CalendarRequestMessage);
    HANDLECLASS(CalendarMessage);
    HANDLECLASS(CalendarFailureMessage);
    HANDLECLASS(InitialDataRequestMessage);
    HANDLECLASS(InitialDataMessage);
    HANDLECLASS(KnownHistoryMessage);
    HANDLECLASS(KnownHistoryRequest);

    void HandleTipRequestMessage(TipRequestMessage request);

    void HandleTipMessage(TipMessage message);

    void HandleCalendarRequestMessage(CalendarRequestMessage request);

    void HandleCalendarFailureMessage(CalendarFailureMessage message);

    void HandleInitialDataRequestMessage(InitialDataRequestMessage request);

    void HandleInitialDataMessage(InitialDataMessage request);

    void HandleCalendarMessage(CalendarMessage calendar_message);

    void HandleKnownHistoryMessage(KnownHistoryMessage known_history_message);

    void HandleKnownHistoryRequest(KnownHistoryRequest known_history_request);

};

void LoadHashesIntoDataStoreFromMessageTypesAndContents(MemoryDataStore &hashdata,
                                                        std::vector<std::string> &types,
                                                        std::vector<vch_t> &contents,
                                                        CreditSystem *credit_system);

#endif //FLEX_DATAMESSAGEHANDLER_H
