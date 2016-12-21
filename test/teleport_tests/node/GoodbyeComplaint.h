#ifndef TELEPORT_GOODBYECOMPLAINT_H
#define TELEPORT_GOODBYECOMPLAINT_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/define.h>
#include <src/crypto/point.h>
#include <src/crypto/signature.h>
#include "Data.h"
#include "Relay.h"

class GoodbyeComplaint
{
public:
    uint160 goodbye_message_hash{0};
    uint8_t key_quarter_holder_position{0};
    std::vector<uint32_t> positions_of_bad_encrypted_key_sixteenths;
    Signature signature;

    static std::string Type() { return "goodbye_complaint"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(goodbye_message_hash);
        READWRITE(key_quarter_holder_position);
        READWRITE(positions_of_bad_encrypted_key_sixteenths);
        READWRITE(signature);
    );

    JSON(goodbye_message_hash, key_quarter_holder_position, positions_of_bad_encrypted_key_sixteenths);

    DEPENDENCIES(goodbye_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetSecretSender(Data data);

    Relay *GetComplainer(Data data);
};


#endif //TELEPORT_GOODBYECOMPLAINT_H
