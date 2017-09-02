#ifndef TELEPORT_TASK_H
#define TELEPORT_TASK_H

#include "define.h"

#define SEND_KEY_DISTRIBUTION_MESSAGE 1
#define SEND_KEY_DISTRIBUTION_AUDIT_MESSAGE 2
#define SEND_SECRET_RECOVERY_MESSAGE 3
#define SEND_RECOVERY_FAILURE_AUDIT_MESSAGE 4
#define SEND_SUCCESSION_COMPLETED_MESSAGE 5
#define SEND_SUCCESSION_COMPLETED_AUDIT_MESSAGE 6


class Task
{
public:
    uint8_t task_type{0};
    uint160 event_hash{0};
    uint32_t position{0};

    Task() { }

    Task(uint8_t task_type, uint160 event_hash):
            task_type(task_type), event_hash(event_hash)
    { }

    Task(uint8_t task_type, uint160 event_hash, uint32_t position):
            task_type(task_type), event_hash(event_hash), position(position)
    { }

    bool operator==(const Task& other)
    {
        return task_type == other.task_type and event_hash == other.event_hash and position == other.position;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(task_type);
        READWRITE(event_hash);
        READWRITE(position);
    );

    JSON(task_type, event_hash, position);

    std::string ToString() { return json(); }

    bool IsInheritable()
    {
        return task_type == SEND_SECRET_RECOVERY_MESSAGE or task_type == SEND_SUCCESSION_COMPLETED_MESSAGE;
    }
};


#endif //TELEPORT_TASK_H
