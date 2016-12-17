#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "RelayState.h"
#include "Relay.h"
#include "RelayJoinMessage.h"
#include "SecretRecoveryMessage.h"
#include "Obituary.h"
#include "Data.h"
#include "RelayExit.h"


using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


TEST(Points, PrecomputeMultiplicationSpeedup)
{
    CBigNum random;
    random.Randomize(Secp256k1Point::Modulus());
    Point point(SECP256K1, random);

    std::vector<std::vector<Point> > precomputed_multiples;
    precomputed_multiples.resize(64);

    for (uint32_t nybble = 0; nybble < 64; nybble++)
    {
        for (uint32_t i = 0; i < 16; i++)
        {
            CBigNum x = i;
            x <<= nybble * 4;
            Point precomputed_multiple = x * point;
            precomputed_multiples[nybble].push_back(precomputed_multiple);
        }
    }

    CBigNum number;
    number.Randomize(Secp256k1Point::Modulus());

    uint64_t start = GetTimeMicros();
    auto first_product = number * point;
    log_ << "normal multiplication in " << GetTimeMicros() - start << " us\n";


    // precomputed multiplication
    auto second_product = Point(SECP256K1, 0);
    start = GetTimeMicros();
    for (uint32_t nybble = 0; nybble < 64; nybble++)
    {
        CBigNum which_multiple = number % 16;
        second_product += precomputed_multiples[nybble][which_multiple.getulong()];
        number /= 16;
    }

    ASSERT_THAT(first_product, Eq(second_product));
    log_ << "precomputed multiplication in " << GetTimeMicros() - start << " us\n";
}
