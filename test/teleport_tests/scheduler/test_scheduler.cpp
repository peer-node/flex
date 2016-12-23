#include <src/base/util_time.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "gmock/gmock.h"
#include "teleportnode/schedule.h"

using namespace ::testing;


MemoryDataStore schedule_test_data;

void SetDone(uint160 hash)
{
    schedule_test_data[hash]["done"] = 1;
}

class AScheduler : public Test
{
public:
    Scheduler scheduler;
    int64_t start_time = 1000000000;
    int64_t task_time = start_time + 1000000;
    uint160 hash = 1;

    virtual void SetUp()
    {
        SetMockTimeMicros(start_time);
    }
};

TEST_F(AScheduler, CanScheduleEvents)
{
    scheduler.AddTask(ScheduledTask("set_done", SetDone));
    scheduler.Schedule("set_done", hash, task_time);

    SetMockTimeMicros(task_time - 1);
    scheduler.DoTasksScheduledForExecutionBeforeNow();

    int is_done = schedule_test_data[hash]["done"];
    ASSERT_THAT(is_done, Eq(0));

    SetMockTimeMicros(task_time);
    scheduler.DoTasksScheduledForExecutionBeforeNow();

    is_done = schedule_test_data[hash]["done"];
    ASSERT_THAT(is_done, Eq(1));
}

class ObjectWithMethod
{
public:
    uint160 recorded_hash{0};

    void SomeMethod(uint160 hash)
    {
        recorded_hash = hash;
    }
};

TEST_F(AScheduler, CanScheduleExecutionOfObjectMethods)
{
    ObjectWithMethod object_with_method;

    ScheduledTask task("set_hash", &ObjectWithMethod::SomeMethod, &object_with_method);
    scheduler.AddTask(task);
    scheduler.Schedule("set_hash", hash, task_time);

    SetMockTimeMicros(task_time - 1);
    scheduler.DoTasksScheduledForExecutionBeforeNow();

    ASSERT_THAT(object_with_method.recorded_hash, Eq(0));

    SetMockTimeMicros(task_time);
    scheduler.DoTasksScheduledForExecutionBeforeNow();

    ASSERT_THAT(object_with_method.recorded_hash, Eq(hash));
}