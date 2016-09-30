#include "EncryptedSecret.h"
#include "crypto/bignum_hashes.h"

EncryptedSecret::EncryptedSecret(CBigNum secret, Point recipient)
{
    point = Point(SECP256K1, secret);
    CBigNum shared_secret = Hash(recipient * secret);
    secret_xor_shared_secret = secret ^ shared_secret;
}

CBigNum EncryptedSecret::RecoverSecret(CBigNum recipient_private_key)
{
    CBigNum shared_secret = Hash(point * recipient_private_key);
    return shared_secret ^ secret_xor_shared_secret;
}

bool EncryptedSecret::Audit(CBigNum secret, Point recipient)
{
    CBigNum shared_secret = Hash(recipient * secret);
    return (shared_secret ^ secret_xor_shared_secret) == secret && point == Point(SECP256K1, secret);
}
