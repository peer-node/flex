#ifndef FLEX_ENCRYPTEDSECRET_H
#define FLEX_ENCRYPTEDSECRET_H


#include <src/crypto/point.h>

class EncryptedSecret
{
public:
    Point point;
    CBigNum secret_xor_shared_secret;

    EncryptedSecret() { }

    EncryptedSecret(CBigNum secret, Point recipient);

    CBigNum RecoverSecret(CBigNum recipient_private_key);

    bool Audit(CBigNum secret, Point recipient);
};


#endif //FLEX_ENCRYPTEDSECRET_H
