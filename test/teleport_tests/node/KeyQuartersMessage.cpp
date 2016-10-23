#include <src/crypto/secp256k1point.h>
#include "KeyQuartersMessage.h"

void KeyQuartersMessage::SetQuartersAndStoreCorrespondingSecrets(Point public_key, MemoryDataStore &keydata)
{
    CBigNum sum_of_first_three_quarters = 0;

    for (uint64_t i = 0; i < 3; i++)
    {
        CBigNum private_key_quarter;
        private_key_quarter.Randomize(Secp256k1Point::Modulus());
        AddQuarterKeyAndStoreCorrespondingSecret(private_key_quarter, keydata);
        sum_of_first_three_quarters += private_key_quarter;
    }

    CBigNum private_key = keydata[public_key]["privkey"];
    CBigNum final_quarter = private_key - sum_of_first_three_quarters;
    AddQuarterKeyAndStoreCorrespondingSecret(final_quarter, keydata);
}

void KeyQuartersMessage::AddQuarterKeyAndStoreCorrespondingSecret(CBigNum secret, MemoryDataStore &keydata)
{
    Point public_key_quarter(SECP256K1, secret);
    public_key_quarters.push_back(public_key_quarter);
    keydata[public_key_quarter]["privkey"] = secret;
}