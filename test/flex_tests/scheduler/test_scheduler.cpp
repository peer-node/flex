#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "../flex_data/TestData.h"
#include "flexnode/schedule.h"

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