#ifndef TELEPORT_ENCRYPTEDKEYQUARTER_H
#define TELEPORT_ENCRYPTEDKEYQUARTER_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/define.h>
#include <array>


class EncryptedKeyQuarter
{
public:
    std::array<uint256, 6> encrypted_key_twentyfourths{{0, 0, 0, 0, 0, 0}};

    IMPLEMENT_SERIALIZE
    (
        READWRITE(encrypted_key_twentyfourths);
    );

    JSON(encrypted_key_twentyfourths);
};


#endif //TELEPORT_ENCRYPTEDKEYQUARTER_H
