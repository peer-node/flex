#include <boost/foreach.hpp>
#include <src/net/netbase.h>
#include "base/util.h"
#include "sync.h"
#include "util_compare.h"
#include "util_log.h"
#include "util_format.h"
#include "ui_interface.h"


using namespace std;


//
// "Never go to sea with two chronometers; take one or three."
// Our three time sources are:
//  - System clock
//  - Median of other nodes clocks
//  - The user (asking the user to fix the system clock if the first two disagree)
//
static int64_t nMockTime = 0;  // For unit testing
int64_t nMockTimeMicros = 0;

int64_t GetTime()
{
    if (nMockTime) return nMockTime;

    return (int64_t) time(NULL);
}

void SetMockTime(int64_t nMockTimeIn)
{
    nMockTime = nMockTimeIn;
}

static CCriticalSection cs_nTimeOffset;
static int64_t nTimeOffset = 0;

void SetTimeOffset(int64_t offset)
{
    LOCK(cs_nTimeOffset);
    nTimeOffset = offset;
}

int64_t GetTimeOffset()
{
    LOCK(cs_nTimeOffset);
    return nTimeOffset;
}

int64_t GetAdjustedTime()
{
    return GetTime() + GetTimeOffset();
}

void AddTimeData(const CNetAddr& ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - GetTime();

    LOCK(cs_nTimeOffset);
    // Ignore duplicates
    static set<CNetAddr> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    static CMedianFilter<int64_t> vTimeOffsets(200,0);
    vTimeOffsets.input(nOffsetSample);
    LogPrintf("Added time data, samples %d, offset %+d (%+d minutes)\n", vTimeOffsets.size(), nOffsetSample, nOffsetSample/60);
    if (vTimeOffsets.size() >= 2)
    {
        int64_t nMedian = vTimeOffsets.median();
        std::vector<int64_t> vSorted = vTimeOffsets.sorted();
        // Only let other nodes change our time by so much
        if (abs64(nMedian) < 70 * 60)
        {
            nTimeOffset = nMedian;
        }
        else
        {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone)
            {
                // If nobody has a time different than ours but within 5 minutes of ours, give a warning
                bool fMatch = false;
                BOOST_FOREACH(int64_t nOffset, vSorted)
                if (nOffset != 0 && abs64(nOffset) < 5 * 60)
                    fMatch = true;

                if (!fMatch)
                {
                    fDone = true;
                    string strMessage = _("Warning: Please check that your computer's date and time are correct! If your clock is wrong Teleport will not work properly.");
                    strMiscWarning = strMessage;
                    LogPrintf("*** %s\n", strMessage);
                }
            }
        }
        if (fDebug)
        {
            BOOST_FOREACH(int64_t n, vSorted)
            LogPrintf("%+d  ", n);
            LogPrintf("|  ");
        }
        LogPrintf("nTimeOffset = %+d  (%+d minutes)\n", nTimeOffset, nTimeOffset/60);
    }
}

