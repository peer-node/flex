#ifndef FLEX_UTIL_TIME_H
#define FLEX_UTIL_TIME_H

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


static int64_t nMockTimeMicros = 0;

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

inline uint64_t GetTimeMicros()
{
    if (nMockTimeMicros != 0)
        return nMockTimeMicros;
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

class CNetAddr;

void AddTimeData(const CNetAddr& ip, int64_t nTime);

#endif //FLEX_UTIL_TIME_H
