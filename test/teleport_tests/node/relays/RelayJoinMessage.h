
#ifndef TELEPORT_RELAYJOINMESSAGE_H
#define TELEPORT_RELAYJOINMESSAGE_H


#include <src/crypto/point.h>
#include <src/crypto/signature.h>
#include <test/teleport_tests/node/credit_handler/MinedCreditMessage.h>
#include <src/crypto/secp256k1point.h>
#include "test/teleport_tests/teleport_data/MemoryDataStore.h"
#include "test/teleport_tests/node/Data.h"
#include "RelayPublicKeySet.h"


inline Point SumOfPoints(std::vector<Point> points)
{
    if (points.size() == 0)
        return Point(SECP256K1, 0);

    Point sum(points[0].curve, 0);
    for (auto point : points)
        sum += point;
    return sum;
}

class RelayJoinMessage
{
public:
    uint160 mined_credit_message_hash{0};
    RelayPublicKeySet public_key_set;
    Signature signature;

    static std::string Type() { return "relay_join"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit_message_hash);
        READWRITE(public_key_set);
        READWRITE(signature);
    )

    JSON(mined_credit_message_hash, public_key_set, signature);

    DEPENDENCIES(mined_credit_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data)
    {
        MinedCreditMessage msg = data.msgdata[mined_credit_message_hash]["msg"];
        Point pubkey;
        pubkey.setvch(msg.mined_credit.keydata);
        return pubkey;
    }

    void PopulatePublicKeySet(MemoryDataStore &keydata)
    {
        public_key_set.Populate(keydata);
        StorePrivateSigningKey(keydata);
    }

    void StorePrivateSigningKey(MemoryDataStore &keydata)
    {
        CBigNum private_key(0);
        for (auto public_key_twentyfourth : public_key_set.PublicKeyTwentyFourths())
        {
            CBigNum private_key_twentyfourth = keydata[public_key_twentyfourth]["privkey"];
            private_key += private_key_twentyfourth;
        }
        keydata[PublicSigningKey()]["privkey"] = private_key % Secp256k1Point::Modulus();
    }

    Point PublicSigningKey()
    {
        return SumOfPoints(public_key_set.PublicKeyTwentyFourths());
    }

    bool ValidateSizes()
    {
        return public_key_set.ValidateSizes();
    }
};


#endif //TELEPORT_RELAYJOINMESSAGE_H
