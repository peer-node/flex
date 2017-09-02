#include <src/vector_tools.h>
#include <src/base/util_time.h>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/relays/structures/RelayState.h"


using namespace ::testing;
using namespace std;

#include "log.h"
#define LOG_CATEGORY "test"


class PrecomputedPointMultiplicationData
{
public:
    std::vector<std::vector<Point> > precomputed_multiples;

    PrecomputedPointMultiplicationData(Point point)
    {
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
    }

    Point EvaluateMultiple(CBigNum number)
    {
        auto multiple = Point(precomputed_multiples[0][0].curve, 0);

        for (uint32_t nybble = 0; nybble < 64; nybble++)
        {
            CBigNum which_multiple = number % 16;
            multiple += precomputed_multiples[nybble][which_multiple.getulong()];
            number /= 16;
        }
        return multiple;
    }
};

TEST(APrecomputedPointMultiplicationDataInstance, SpeedsUpPointMultiplication)
{
    CBigNum random;
    random.Randomize(Secp256k1Point::Modulus());
    Point point(SECP256K1, random);

    PrecomputedPointMultiplicationData precomputed_data(point);

    CBigNum number;
    number.Randomize(Secp256k1Point::Modulus());

    Point first_product;
    uint64_t start = GetTimeMicros();
    first_product = number * point;
    log_ << "normal multiplication in " << GetTimeMicros() - start << " us\n";

    start = GetTimeMicros();
    auto second_product = precomputed_data.EvaluateMultiple(number);
    log_ << "precomputed multiplication in " << GetTimeMicros() - start << " us\n";

    ASSERT_THAT(first_product, Eq(second_product));
}
