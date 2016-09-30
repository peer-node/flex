#ifndef TELEPORT_RESOURCEMONITOR
#define TELEPORT_RESOURCEMONITOR

#define MAX_MESSAGE_RATE 50                 // per second
#define MAX_BANDWIDTH 33000                 // bytes per second
#define BANDWIDTH_AVERAGING_TIME 2000000    // microseconds
#define MESSAGE_RATE_AVERAGING_TIME 2000000 // microseconds
#define TIME_USAGE_AVERAGING_TIME 2000000   // microseconds

#include "log.h"
#define LOG_CATEGORY "resourcemonitor.h"


class InitialDataMeasurer
{
public:
    uint64_t maximum_initial_data_size;
    std::map<uint64_t, uint64_t> received;

    InitialDataMeasurer()
    {
        uint64_t age_in_seconds = GetTimeMicros() / 1e6 - 1437346800;
        uint64_t age_in_days = age_in_seconds / 86400;
        uint64_t kilobyte = 1024;
        uint64_t megabyte = 1024 * kilobyte;
        maximum_initial_data_size = 50 * megabyte + 2 * age_in_days * kilobyte;
    }

    bool AcceptMessage(CNode *peer, uint64_t size)
    {
        uint64_t peer_id = peer->GetId();
        if (received.count(peer_id) == 0)
            received[peer_id] = 0;
        received[peer_id] += size;
        return received[peer_id] <= maximum_initial_data_size;
    }
};

class BandwidthCalculator
{
public:
    double estimated_bandwidth;
    uint64_t estimate_time;

    BandwidthCalculator():
        estimated_bandwidth(0),
        estimate_time(GetTimeMicros())
    { }

    void RecordUsage(uint64_t size)
    {
        uint64_t now = GetTimeMicros();
        uint64_t interval = now - estimate_time;
        double new_rate = size * 1.0 / interval;
        double old_weighting = exp(-interval * 1.0 / BANDWIDTH_AVERAGING_TIME);
        double new_weighting = 1 - old_weighting;
        estimated_bandwidth = old_weighting * estimated_bandwidth
                                + new_weighting * new_rate;
        estimate_time = now;
    }

    double LatestEstimate() const
    {
        uint64_t now = GetTimeMicros();
        uint64_t interval = now - estimate_time;
        double weighting = exp(-interval * 1.0 / BANDWIDTH_AVERAGING_TIME);
        return weighting * estimated_bandwidth;
    }
};

class MessageRateCalculator
{
public:
    double estimated_rate;
    uint64_t estimate_time;

    MessageRateCalculator():
        estimated_rate(0),
        estimate_time(GetTimeMicros())
    { }

    void RecordMessage()
    {
        uint64_t now = GetTimeMicros();
        uint64_t interval = now - estimate_time;
        double new_rate = 1.0 / interval;
        double old_weighting = exp(-interval * 1.0 
                                    / MESSAGE_RATE_AVERAGING_TIME);
        double new_weighting = 1 - old_weighting;
        estimated_rate = old_weighting * estimated_rate
                          + new_weighting * new_rate;
        estimate_time = now;
    }

    double LatestEstimate() const
    {
        uint64_t now = GetTimeMicros();
        uint64_t interval = now - estimate_time;
        double weighting = exp(-interval * 1.0 / MESSAGE_RATE_AVERAGING_TIME);
        return weighting * estimated_rate;
    }
};

class TimeUsageCalculator
{
public:
    double estimated_time_usage_fraction;
    uint64_t estimate_time;

    TimeUsageCalculator():
        estimated_time_usage_fraction(0),
        estimate_time(GetTimeMicros())
    { }

    void RecordTimeTaken(uint64_t time_taken)
    {
        uint64_t now = GetTimeMicros();
        uint64_t interval = now - estimate_time;
        double new_fraction = time_taken * 1.0 / interval;
        double old_weighting = exp(-interval * 1.0
                                    / TIME_USAGE_AVERAGING_TIME);
        double new_weighting = 1 - old_weighting;
        estimated_time_usage_fraction 
            = old_weighting * estimated_time_usage_fraction
                 + new_weighting * new_fraction;
        estimate_time = now;
    }

    double LatestEstimate() const
    {
        uint64_t now = GetTimeMicros();
        uint64_t interval = now - estimate_time;
        double weighting = exp(-interval * 1.0 / TIME_USAGE_AVERAGING_TIME);
        return weighting * estimated_time_usage_fraction;
    }
};

class ResourceMonitor
{
public:
    BandwidthCalculator bandwidth_calculator;
    MessageRateCalculator message_rate_calculator;
    TimeUsageCalculator time_usage_calculator;

    void RecordMessage(uint64_t size)
    {
        message_rate_calculator.RecordMessage();
        bandwidth_calculator.RecordUsage(size);
    }

    void RecordTimeTaken(uint64_t time_taken)
    {
        time_usage_calculator.RecordTimeTaken(time_taken);
    }

    double Bandwidth() const
    {
        return bandwidth_calculator.LatestEstimate();
    }

    double MessageRate() const
    {
        return message_rate_calculator.LatestEstimate();
    }

    double FractionOfTimeUsed() const
    {
        return time_usage_calculator.LatestEstimate();
    }
};


typedef std::map<uint64_t, ResourceMonitor> ResourceMonitorMap;

class IncomingResourceLimiter
{
public:
    ResourceMonitorMap resource_monitors;

    IncomingResourceLimiter() { }

    ResourceMonitor& Monitor(CNode *peer)
    {
        uint64_t peer_id = peer->GetId();

        if (resource_monitors.count(peer_id) == 0)
            resource_monitors[peer_id] = ResourceMonitor();

        return resource_monitors[peer_id];
    }

    bool AcceptMessage(CNode *peer, uint64_t size)
    {
        Record(peer, size);
        if (LooksLikeDoS(peer))
            return false;
        return true;
    }

    void Record(CNode *peer, uint64_t size)
    {
        Monitor(peer).RecordMessage(size);
    }

    void RecordTimeTaken(CNode *peer, uint64_t time_taken)
    {
        Monitor(peer).RecordTimeTaken(time_taken);
    }

    double TotalMessageRate()
    {
        double total_rate = 0;
        foreach_(const ResourceMonitorMap::value_type& item, resource_monitors)
            total_rate += item.second.MessageRate();
        return total_rate;
    }

    double TotalBandwidth()
    {
        double total_bandwidth = 0;
        foreach_(const ResourceMonitorMap::value_type& item, resource_monitors)
            total_bandwidth += item.second.Bandwidth();
        return total_bandwidth;
    }

    double FractionOfTimeSpentBusy()
    {
        double busy_fraction = 0;
        foreach_(const ResourceMonitorMap::value_type& item, resource_monitors)
            busy_fraction += item.second.FractionOfTimeUsed();
        return busy_fraction;
    }

    void TrimNodes()
    {
        std::map<uint64_t, bool> still_here;
        std::vector<uint64_t> to_remove;
        foreach_(CNode* peer, vNodes)
            still_here[peer->GetId()] = true;
        foreach_(const ResourceMonitorMap::value_type& item, resource_monitors)
            if (!still_here.count(item.first))
            {
                to_remove.push_back(item.first);
            }
        foreach_(const uint64_t peer_id, to_remove)
        {
            resource_monitors.erase(peer_id);
        }
    }

    bool Strained()
    {
        return TotalMessageRate() > 0.9 * MAX_MESSAGE_RATE  ||
               TotalBandwidth() > 0.9 * MAX_BANDWIDTH       ||
               FractionOfTimeSpentBusy() > 0.9;
    }

    bool LooksLikeDoS(CNode *peer)
    {
        TrimNodes();

        if (!Strained())
            return false;

        uint64_t num_peers = resource_monitors.size();

        if (num_peers == 1)
            return true;

        double peer_bandwidth_share = Monitor(peer).Bandwidth() 
                                       / TotalBandwidth();

        double peer_msg_share = Monitor(peer).MessageRate() 
                                 / TotalMessageRate();

        double peer_time_share = Monitor(peer).FractionOfTimeUsed();

        double fraction_hogged = max(peer_bandwidth_share,
                                     max(peer_msg_share, peer_time_share));

        if (peer->fInbound)
        {
            if (num_peers < 4)
                return fraction_hogged > 0.8;

            return fraction_hogged > 2.0 / num_peers;
        }
        if (num_peers < 4)
            return fraction_hogged > 0.9;

        return fraction_hogged > 0.6;
    }
};


class Priority
{
public:
    uint160 priority_data;

    Priority() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(priority_data);
    )

    string_t ToString()
    {
        return priority_data.ToString();
    }

    Priority(uint160 work_done, uint64_t timestamp)
    {
        priority_data = work_done;
        priority_data <<= 64;
        timestamp = (1ULL << 63) - timestamp;
        priority_data += timestamp;
    }
};


class OutgoingResourceLimiter
{
public:
    bool running;

    void ForwardMessage(string_t type, uint160 hash);

    void ForwardTransaction(uint160 hash);

    OutgoingResourceLimiter(): running(false)
    {
        ClearQueue();
        StartRunning();
    }

    ~OutgoingResourceLimiter()
    {
        running = false;
    }

    void ClearQueue();

    void StartRunning()
    {
        running = true;
        boost::thread t(&OutgoingResourceLimiter::DoForwarding, this);
    }

    void ScheduleForwarding(uint160 msg_hash, uint160 work)
    {
        Priority priority(work, GetTimeMicros());
        msgdata[msg_hash].Location("forwarding") = priority;
    }

    void SleepMilliseconds(uint64_t milliseconds)
    {
        boost::this_thread::interruption_point();
        boost::this_thread::sleep(boost::posix_time::milliseconds(milliseconds));
    }

    void DoForwarding();
};

extern OutgoingResourceLimiter outgoing_resource_limiter;

#endif