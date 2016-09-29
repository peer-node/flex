#ifndef FLEX_CALENDARHANDLER_H
#define FLEX_CALENDARHANDLER_H


#include "DataMessageHandler.h"


class CalendarHandler
{
public:
    MemoryDataStore &msgdata, &creditdata;
    DataMessageHandler *data_message_handler;

    uint64_t calendar_scrutiny_time{CALENDAR_SCRUTINY_TIME};
    uint64_t number_of_megabytes_for_mining{FLEX_WORK_NUMBER_OF_MEGABYTES};

    CalendarHandler(DataMessageHandler *data_message_handler_) :
        data_message_handler(data_message_handler_),
        msgdata(data_message_handler_->msgdata),
        creditdata(data_message_handler_->creditdata)
    { }

    void HandleCalendarRequestMessage(CalendarRequestMessage request);

    void HandleCalendarFailureMessage(CalendarFailureMessage message);

    void HandleCalendarMessage(CalendarMessage calendar_message);

    bool ScrutinizeCalendarWithTheMostWork();

    bool CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar);

    bool Scrutinize(Calendar calendar, uint64_t scrutiny_time, CalendarFailureDetails &details);

    void HandleIncomingCalendar(CalendarMessage incoming_calendar);

    void RequestInitialDataMessage(CalendarMessage calendar_message);

    bool ValidateCalendarMessage(CalendarMessage calendar_message);

    bool ValidateCalendarFailureMessage(CalendarFailureMessage message);

    void MarkCalendarsAsInvalidAndSwitchToNewCalendar(CalendarFailureMessage message);

    void SendCalendarFailureMessageInResponseToCalendarMessage(CalendarMessage calendar_message);

    CalendarFailureDetails GetCalendarFailureDetails(Calendar &calendar_);

};


#endif //FLEX_CALENDARHANDLER_H
