#ifndef FLEX_ENCRYPTEDSECRET_H
#define FLEX_ENCRYPTEDSECRET_H


#include <src/crypto/point.h>

class EncryptedSecret
{
public:
    EncryptedSecret() { }

    EncryptedSecret(CBigNum secret, Point recipient);

    Point point;
    CBigNum secret_xor_shared_secret;

    CBigNum RecoverSecret(CBigNum recipient_private_key);

    bool Audit(CBigNum secret, Point recipient);
};


#endif //FLEX_ENCRYPTEDSECRET_H
