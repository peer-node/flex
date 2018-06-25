#include "gmock/gmock.h"
#include "crypto/point.h"

using namespace ::testing;

class APoint : public Test
{
public:
    Point p;

};

TEST_F(APoint, HasSecp256K1CurveWhenCreatedWithDefaultConstructor)
{
    ASSERT_THAT(p.curve, Eq(SECP256K1));
}

TEST_F(APoint, HasSpecifiedCurveWhenInitializedWithCurveSpecifier)
{
    p = Point(SECP256K1);
    ASSERT_THAT(p.curve, Eq(SECP256K1));
}

TEST(Points, CanBeAddedTogetherToGetAnotherPoint)
{
    auto p = Point(SECP256K1, 0) + Point(SECP256K1, 1);
    ASSERT_THAT(p.curve, Eq(SECP256K1));
}