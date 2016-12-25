#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Hello
#include <boost/test/unit_test.hpp>
#include "database/DiskDataStore.h"


BOOST_AUTO_TEST_CASE(TestDatastore)
{
    CLevelDBWrapper teleportdb("db", 1 << 29);
    CDataStore encdata(&teleportdb, "E", "pw1");

    CDataStore encdatabad(&teleportdb, "E", "pw2");

    CDataStore encdatagood(&teleportdb, "E", "pw1");

    uint256 x = 256;

    encdata[x]["hello"] = string_t("true");

    string_t str = encdata[x]["hello"];

    string_t strbad = encdatabad[x]["hello"];
    string_t strgood = encdatagood[x]["hello"];

    BOOST_CHECK(str == "true");
    BOOST_CHECK(strgood == "true");
    BOOST_CHECK(strbad != "true");

    uint160 n_ = 1;
    for (uint64_t i = 0; i < 155; i++)
    {
        encdata[i].Location("num") = n_;
        n_ = n_ << 1;
    }

    uint64_t l = 1;
    for (uint64_t i = 0; i < 62; i++)
    {
        encdata[i].Location("m") = l;
        l = l << 1;
    }

    for (uint64_t i = 0; i < 1500; i++)
    {
        encdata[i].Location("n") = i;
    }
    CLocationIterator<> mscanner = encdata.LocationIterator("m");
    uint64_t s,r;

    while (mscanner.GetNextObjectAndLocation(s, r))
    {
        printf("found %lu at location %lu\n", s, r);
    }


    
    CLocationIterator<> numscanner = encdata.LocationIterator("num");

    uint160 loc;
    uint64_t num, num2;

    uint64_t num_found = 0;
    while (numscanner.GetNextObjectAndLocation(num, loc))
    {
        BOOST_CHECK(loc >> num == 1);
        num_found++;
    }
    BOOST_CHECK(num_found == 155);
    printf("found: %lu\n", num_found);

    encdata.GetObjectAtLocation((uint64_t)147, "n", num);
    BOOST_CHECK(num == 147);
    num = 0;

    encdata.RemoveFromLocation("n", (uint64_t)147);
    encdata.GetObjectAtLocation((uint64_t)147, "n", num);
    BOOST_CHECK(num != 147);

    CLocationIterator<> nscanner = encdata.LocationIterator("n");    
    num_found = 0;
    while (nscanner.GetNextObjectAndLocation(num, num2))
    {
        BOOST_CHECK(num == num2);
        num_found++;
    }
    BOOST_CHECK(num_found == 1500 - 1);
    printf("found: %lu\n", num_found);
}
