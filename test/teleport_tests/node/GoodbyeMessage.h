#ifndef TELEPORT_GOODBYEMESSAGE_H
#define TELEPORT_GOODBYEMESSAGE_H


#include <src/crypto/signature.h>
#include "Data.h"

class Relay;


class GoodbyeMessage
{
public:
    uint64_t dead_relay_number{0};
    uint64_t successor_relay_number{0};
    std::vector<uint64_t> key_quarter_sharers;
    std::vector<uint8_t> key_quarter_positions;
    std::vector<std::vector<CBigNum> > encrypted_key_sixteenths;

    Signature signature;

    static std::string Type() { return "goodbye"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(dead_relay_number);
        READWRITE(successor_relay_number);
        READWRITE(key_quarter_sharers);
        READWRITE(key_quarter_positions);
        READWRITE(encrypted_key_sixteenths);
        READWRITE(signature);
    );

    JSON(dead_relay_number, successor_relay_number, key_quarter_sharers, key_quarter_positions,
         encrypted_key_sixteenths, signature);

    DEPENDENCIES();

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    void PopulateEncryptedKeySixteenths(Data data);

    void PopulateFourEncryptedKeySixteenths(Relay *sharer, Relay *successor, uint8_t position, Data data);

    Relay *GetSuccessor(Data data);

    bool ExtractSecrets(Data data);

    bool DecryptFourKeySixteenths(Relay *sharer, Relay *successor, uint8_t quarter_holder_position,
                                  uint32_t key_sharer_position, Data data);
};


#endif //TELEPORT_GOODBYEMESSAGE_H
