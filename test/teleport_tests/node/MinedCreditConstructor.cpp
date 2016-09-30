#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include <src/crypto/bignum.h>
#include <src/crypto/point.h>
#include <src/crypto/secp256k1point.h>
#include "MinedCreditConstructor.h"


MinedCredit MinedCreditConstructor::ConstructMinedCredit()
{
    MinedCredit mined_credit;
    mined_credit.amount = ONE_CREDIT;
    Point public_key = NewPublicKey();
    mined_credit.keydata = public_key.getvch();
    return mined_credit;
}

Point MinedCreditConstructor::NewPublicKey()
{
    CBigNum private_key;
    private_key.Randomize(Secp256k1Point::Modulus());
    Point public_key(SECP256K1, private_key);
    keydata[public_key]["privkey"] = private_key;
    return public_key;
}