#ifndef TELEPORT_SCHEDULE
#define TELEPORT_SCHEDULE

#include "define.h"
#include "base/util_time.h"
#include <boost/thread.hpp>
#include "crypto/uint256.h"
#include "vector_tools.h"
#include <functional>

#include "log.h"
#define LOG_CATEGORY "schedule.h"

class ScheduledTask
{
public:
    string_t task_type;
    MemoryDataStore *scheduledata;
    MemoryLocationIterator schedule_scanner;
    std::function<void(uint160)> task_function;

    ScheduledTask() { }

    template <typename FUNCTION>
    ScheduledTask(const char* task_type, FUNCTION method):
            task_type(task_type),
            task_function(method)
    {
    }

    template <typename METHOD, typename OBJECT>
    ScheduledTask(const char* task_type, METHOD method, OBJECT object):
            task_type(task_type)
    {
        task_function = std::bind(method, object, std::placeholders::_1);
    }

    void Initialize(MemoryDataStore &scheduledata_)
    {
        schedule_scanner = scheduledata_.LocationIterator(task_type);
        scheduledata = &scheduledata_;
        ClearTasks();
    }

    void DoTask(uint160 task_hash)
    {
        task_function(task_hash);
    }

    void ClearTasks()
    {
        uint64_t scheduled_time = 0;
        std::vector<uint160> task_hashes;
        schedule_scanner.SeekEnd();
        
        while (schedule_scanner.GetPreviousObjectAndLocation(task_hashes, scheduled_time))
        {
            scheduledata->RemoveFromLocation(task_type, scheduled_time);
            schedule_scanner = scheduledata->LocationIterator(task_type);
            schedule_scanner.SeekEnd();
        }
    }

    void DoTasksScheduledForExecutionBeforeNow()
    {
        uint64_t now = GetTimeMicros();
        uint64_t scheduled_time = 0;
        std::vector<uint160> task_hashes;
        schedule_scanner = scheduledata->LocationIterator(task_type);

        schedule_scanner.SeekStart();

        while (schedule_scanner.GetNextObjectAndLocation(task_hashes, scheduled_time))
        {
            if (scheduled_time > now)
                break;
            
            scheduledata->RemoveFromLocation(task_type, scheduled_time);

            for (auto task_hash : task_hashes)
            {
                try
                {
                    DoTask(task_hash);
                }
                catch (const std::runtime_error &e)
                {
                    log_ << "encountered runtime error: " << e.what()
                         << " during scheduled execution of " << task_type
                         << " with task " << task_hash << "\n";
                }
                catch (...)
                {
                    log_ << "encountered unknown error "
                         << " during scheduled execution of " << task_type
                         << " with task " << task_hash << "\n";
                }
            }

            schedule_scanner = scheduledata->LocationIterator(task_type);
            schedule_scanner.SeekStart();
        }
    }
};

class Scheduler
{
public:
    MemoryDataStore scheduledata;
    std::vector<ScheduledTask> tasks;
    boost::thread *running_thread{NULL};
    bool running;

    Scheduler()
    {
        running_thread = new boost::thread(&Scheduler::DoTasks, this);
        running = true;
    }

    ~Scheduler()
    {
        Stop();
    }

    void Stop()
    {
        running = false;
        if (running_thread != NULL)
        {
            running_thread->interrupt();
            running_thread->join();
        }
    }

    void DoTasks()
    {
        while (running)
        {
            DoTasksScheduledForExecutionBeforeNow();
            for (uint32_t i = 0; i < 10; i++)
            {
                boost::this_thread::interruption_point();
                boost::this_thread::sleep(boost::posix_time::milliseconds(10));
            }
        }
    }

    void DoTasksScheduledForExecutionBeforeNow()
    {
        for (auto &task : tasks)
            task.DoTasksScheduledForExecutionBeforeNow();
    }

    void AddTask(ScheduledTask task)
    {
        task.Initialize(scheduledata);
        tasks.push_back(task);
    }

    void Schedule(string_t task_type, uint160 task_hash, int64_t when)
    {
        auto scheduled_tasks = TasksScheduledForTime(task_type, when);
        scheduled_tasks.push_back(task_hash);
        scheduledata[scheduled_tasks].Location(task_type) = when;
    }

    bool TaskIsScheduledForTime(string_t task_type, uint160 task_hash, int64_t when)
    {
        return VectorContainsEntry(TasksScheduledForTime(task_type, when), task_hash);
    }

    bool TaskIsScheduledForTime(const char* task_type, uint160 task_hash, int64_t when)
    {
        return TaskIsScheduledForTime(string_t(task_type), task_hash, when);
    }

    std::vector<uint160> TasksScheduledForTime(string_t task_type, int64_t when)
    {
        std::vector<uint160> tasks_at_location;
        scheduledata.GetObjectAtLocation(tasks_at_location, task_type, when);
        return tasks_at_location;
    }
};


#endif