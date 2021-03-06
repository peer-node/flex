#ifndef TELEPORT_CALENDARHANDLER_H
#define TELEPORT_CALENDARHANDLER_H


#include "DataMessageHandler.h"


class CalendarHandler
{
public:
    MemoryDataStore &msgdata, &creditdata;
    DataMessageHandler *data_message_handler;
    CreditSystem *credit_system;

    uint64_t calendar_scrutiny_time{CALENDAR_SCRUTINY_TIME};
    uint64_t number_of_megabytes_for_mining{TELEPORT_WORK_NUMBER_OF_MEGABYTES};

    CalendarHandler(DataMessageHandler *data_message_handler_) :
        data_message_handler(data_message_handler_),
        msgdata(data_message_handler_->msgdata),
        creditdata(data_message_handler_->creditdata),
        credit_system(data_message_handler_->credit_system)
    { }

    void HandleCalendarRequestMessage(CalendarRequestMessage request);

    void HandleCalendarMessage(CalendarMessage calendar_message);

    bool ScrutinizeCalendarWithTheMostWork();

    bool CheckDifficultiesRootsAndProofsOfWork(Calendar &calendar);

    void HandleIncomingCalendar(CalendarMessage incoming_calendar);

    void RequestInitialDataMessage(CalendarMessage calendar_message);

    bool ValidateCalendarMessage(CalendarMessage calendar_message);

};


#endif //TELEPORT_CALENDARHANDLER_H
