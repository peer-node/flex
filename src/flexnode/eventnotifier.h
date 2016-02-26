#ifndef FLEX_EVENTNOTIFIER
#define FLEX_EVENTNOTIFIER

#include "database/data.h"
#include <boost/thread.hpp>

#include "json/json_spirit_reader.h"
#include "json/json_spirit.h"

#include "log.h"
#define LOG_CATEGORY "eventnotifier.h"


void NotifyGuiOfBalance(vch_t currency, double balance);

json_spirit::Object GetNotificationFromEventHash(uint160 event_hash);

bool SendNotifications(const json_spirit::Array& notifications);


class EventNotifier
{
public:
    bool running;
    CLocationIterator<> event_scanner;

    EventNotifier()
    {
        boost::thread t(&EventNotifier::DoTasks, this);
        running = true;
    }

    ~EventNotifier()
    {
        running = false;
    }

    void RecordEvent(uint160 event_hash)
    {
        guidata[event_hash].Location("events") = GetTimeMicros();
    }

    void DoTasks()
    {
        while (running)
        {
            SendNotifications();
            boost::this_thread::interruption_point();
            boost::this_thread::sleep(boost::posix_time::milliseconds(500));
        }
    }

    void SendNotifications()
    {
        uint64_t now = GetTimeMicros();
        uint64_t event_time = 0;
        uint160 event_hash;
        event_scanner = guidata.LocationIterator("events");
        event_scanner.Seek(now, true);
        json_spirit::Array notifications;

        while (event_scanner.GetPreviousObjectAndLocation(event_hash,
                                                          event_time))
        {
            guidata.RemoveFromLocation("events", event_time);
            event_scanner = guidata.LocationIterator("events");
            event_scanner.Seek(now, true);

            json_spirit::Object notification;
            notification = GetNotificationFromEventHash(event_hash);
            notifications.push_back(notification);
        }
        
        if (notifications.size() > 0)
            if (!::SendNotifications(notifications))
            {
                log_ << "Bad response from gui. Ending event notifications.\n";
                running = false;
            }
    }
};

#endif