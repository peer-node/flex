#ifndef FLEX_UTIL_TIME_H
#define FLEX_UTIL_TIME_H

#include <cstdint>
#include <boost/date_time/posix_time/posix_time.hpp>
//#include <boost/date_time/microsec_time_clock.hpp>

inline void MilliSleep(int64_t n)
{
// Boost's sleep_for was uninterruptable when backed by nanosleep from 1.50
// until fixed in 1.52. Use the deprecated sleep method for the broken case.
// See: https://svn.boost.org/trac/boost/ticket/7238
#if defined(HAVE_WORKING_BOOST_SLEEP_FOR)
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#elif defined(HAVE_WORKING_BOOST_SLEEP)
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
#else
//should never get here
//#error missing boost sleep implementation
#endif
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

inline int64_t GetTimeMicros()
{
    if (nMockTimeMicros != 0)
        return nMockTimeMicros;
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970,1,1))).total_microseconds();
}

class CNetAddr;

void AddTimeData(const CNetAddr& ip, int64_t nTime);

#endif //FLEX_UTIL_TIME_H
