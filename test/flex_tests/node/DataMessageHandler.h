#ifndef FLEX_DATAMESSAGEHANDLER_H
#define FLEX_DATAMESSAGEHANDLER_H


#include <test/flex_tests/flex_data/MemoryDataStore.h>
#include "MessageHandlerWithOrphanage.h"
#include "TipRequestMessage.h"
#include "CreditSystem.h"
#include "Calendar.h"
#include "TipMessage.h"
#include "CalendarRequestMessage.h"
#include "CalendarMessage.h"
#include "CalendarFailureDetails.h"
#include "InitialDataRequestMessage.h"
#include "InitialDataMessage.h"
#include "CalendarFailureMessage.h"


#define CALENDAR_SCRUTINY_TIME 5000000 // microseconds

class FlexNetworkNode;

class DataMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &msgdata, &creditdata;
    CreditSystem *credit_system{NULL};
    Calendar *calendar{NULL};
    FlexNetworkNode *flex_network_node{NULL};
    uint64_t calendar_scrutiny_time{CALENDAR_SCRUTINY_TIME};
    uint64_t number_of_megabytes_for_mining{FLEX_WORK_NUMBER_OF_MEGABYTES};
    uint160 initial_difficulty{INITIAL_DIFFICULTY};
    uint160 initial_diurnal_difficulty{INITIAL_DIURNAL_DIFFICULTY};

    DataMessageHandler(MemoryDataStore &msgdata_, MemoryDataStore &creditdata_):
            MessageHandlerWithOrphanage(msgdata_), msgdata(msgdata_), creditdata(creditdata_)
    {
        channel = std::string("data");
    }

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(TipRequestMessage);
        HANDLESTREAM(TipMessage);
        HANDLESTREAM(CalendarRequestMessage);
        HANDLESTREAM(CalendarMessage);
        HANDLESTREAM(InitialDataRequestMessage);
        HANDLESTREAM(InitialDataMessage);
        HANDLESTREAM(CalendarFailureMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(TipRequestMessage);
        HANDLEHASH(TipMessage);
        HANDLEHASH(CalendarRequestMessage);
        HANDLEHASH(CalendarMessage);
        HANDLEHASH(InitialDataRequestMessage);
        HANDLEHASH(InitialDataMessage);
        HANDLEHASH(CalendarFailureMessage);
    }

    HANDLECLASS(TipRequestMessage);
    HANDLECLASS(TipMessage);
    HANDLECLASS(CalendarRequestMessage);
    HANDLECLASS(CalendarMessage);
    HANDLECLASS(InitialDataRequestMessage);
    HANDLECLASS(InitialDataMessage);
    HANDLECLASS(CalendarFailureMessage);

    void HandleTipRequestMessage(TipRequestMessage request);

    void HandleTipMessage(TipMessage message);

    void HandleCalendarRequestMessage(CalendarRequestMessage request);

    void HandleCalendarFailureMessage(CalendarFailureMessage message);

    void HandleInitialDataRequestMessage(InitialDataRequestMessage request);

    void HandleInitialDataMessage(InitialDataMessage request);

    void SetCreditSystem(CreditSystem *credit_system_);

    void SetCalendar(Calendar *calendar_);

    void SetNetworkNode(FlexNetworkNode *flex_network_node_);

    uint160 RequestTips();

    uint160 RequestCalendarOfTipWithTheMostWork();

    TipMessage TipWithTheMostWork();

    uint160 SendCalendarRequestToPeer(CNode *peer, MinedCredit mined_credit);

    std::vector<uint160> TipMessageHashesWithTheMostWork();

    void RecordReportedTotalWorkOfTip(uint160 tip_message_hash, uint160 total_work);

    void HandleCalendarMessage(CalendarMessage calendar_message);

    bool ScrutinizeCalendarWithTheMostWork();

    bool CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar);

    bool Scrutinize(Calendar calendar, uint64_t scrutiny_time, CalendarFailureDetails &details);

    void HandleIncomingCalendar(CalendarMessage incoming_calendar);

    bool ValidateCalendarMessage(CalendarMessage calendar_message);

    void RequestInitialDataMessage(CalendarMessage calendar_message);

    bool CheckSpentChainInInitialDataMessage(InitialDataMessage message);

    bool EnclosedMessagesArePresentInInitialDataMessage(InitialDataMessage &message);

    MemoryDataStore GetEnclosedMessageHashes(InitialDataMessage &message);

    bool InitialDataMessageMatchesRequestedCalendar(InitialDataMessage &initial_data_message);

    Calendar GetRequestedCalendar(InitialDataMessage &initial_data_message);

    bool InitialDataMessageMatchesCalendar(InitialDataMessage &message, Calendar calendar);

    bool SequenceOfMinedCreditMessagesIsValidAndHasValidProofsOfWork(std::vector<MinedCreditMessage> vector);

    bool SequenceOfMinedCreditMessagesIsValid(std::vector<MinedCreditMessage> msgs);

    bool SequenceOfMinedCreditMessagesHasValidProofsOfWork(std::vector<MinedCreditMessage> msgs);

    bool ValidateMinedCreditMessagesInInitialDataMessage(InitialDataMessage initial_data_message);

    void StoreDataFromInitialDataMessageInCreditSystem(InitialDataMessage &initial_data_message,
                                                       CreditSystem &credit_system);

    void TrimLastDiurnFromCalendar(Calendar &calendar, CreditSystem *credit_system);

    void SetMiningParametersForInitialDataMessageValidation(uint64_t number_of_megabytes, uint160 initial_difficulty,
                                                            uint160 initial_diurnal_difficulty);

    bool TipOfCalendarMatchesTipOfInitialDataMessage(Calendar &calendar, InitialDataMessage &message);

    void StoreMessageInCreditSystem(std::string type, vch_t content, CreditSystem &credit_system);

    bool ValidateInitialDataMessage(InitialDataMessage initial_data_message);

    void UseInitialDataMessageAndCalendar(InitialDataMessage initial_data_message, Calendar new_calendar);

    void MarkMinedCreditMessagesInInitialDataMessageAsValidated(InitialDataMessage initial_data_message);

    bool ValidateCalendarFailureMessage(CalendarFailureMessage message);

    void MarkCalendarsAsInvalidAndSwitchToNewCalendar(CalendarFailureMessage message);

    Calendar GetCalendarFromCalendarFailureMessage(CalendarFailureMessage message);

    void SendCalendarFailureMessageInResponseToCalendarMessage(CalendarMessage calendar_message);

    CalendarFailureDetails GetCalendarFailureDetails(Calendar &calendar_);
};

void LoadHashesIntoDataStoreFromMessageTypesAndContents(MemoryDataStore &hashdata,
                                                        std::vector<std::string> &types,
                                                        std::vector<vch_t> &contents,
                                                        CreditSystem *credit_system);

#endif //FLEX_DATAMESSAGEHANDLER_H
