#ifndef TELEPORT_UTIL_TIME_H
#define TELEPORT_UTIL_TIME_H

#include <cstdint>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>


inline void MilliSleep(int64_t n)
{
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
}

int64_t GetTime();
void SetMockTime(int64_t nMockTimeIn);
int64_t GetAdjustedTime();
int64_t GetTimeOffset();
void SetTimeOffset(int64_t offset);


extern int64_t nMockTimeMicros;

inline void SetMockTimeMicros(int64_t nMockTimeMicros_)
{
    nMockTimeMicros = nMockTimeMicros_;
}

inline int64_t GetTimeMillis()
{
    if (nMockTimeMicros != 0)
        return nMockTimeMicros / 1000;
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_milliseconds();
}

inline uint64_t GetRealTimeMicros()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

inline uint64_t GetTimeMicros()
{
    if (nMockTimeMicros != 0)
        return nMockTimeMicros;
    return GetRealTimeMicros();
}

inline uint64_t GetAdjustedTimeMicros()
{
    return GetTimeMicros() + 1000000 * (GetAdjustedTime() - GetTime());
}

class CNetAddr;

void AddTimeData(const CNetAddr& ip, int64_t nTime);

#endif //TELEPORT_UTIL_TIME_H
