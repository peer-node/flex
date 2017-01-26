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
    uint32_t key_sharer_position{0};
    uint32_t position_of_bad_encrypted_key_sixteenth{0};
    uint256 recipient_private_key{0};
    Signature signature;

    static std::string Type() { return "goodbye_complaint"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(goodbye_message_hash);
        READWRITE(key_sharer_position);
        READWRITE(position_of_bad_encrypted_key_sixteenth);
        READWRITE(recipient_private_key);
        READWRITE(signature);
    );

    JSON(goodbye_message_hash, key_sharer_position, position_of_bad_encrypted_key_sixteenth,
         recipient_private_key, signature);

    DEPENDENCIES(goodbye_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    Relay *GetSecretSender(Data data);

    Relay *GetComplainer(Data data);

    GoodbyeMessage GetGoodbyeMessage(Data data);

    void Populate(GoodbyeMessage &goodbye_message, Data data);

    void PopulateRecipientPrivateKey(Data data);

    Relay *GetSuccessor(Data data);

    Relay *GetKeySharer(Data data);

    bool IsValid(Data data);

    bool PrivateReceivingKeyIsIncorrect(Data data);

    Point GetPointCorrespondingToSecret(Data data);

    bool EncryptedSecretIsOk(Data data);

    uint256 GetEncryptedSecret(Data data);
};


#endif //TELEPORT_GOODBYECOMPLAINT_H
