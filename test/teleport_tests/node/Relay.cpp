#include <src/crypto/secp256k1point.h>
#include "Relay.h"

KeyQuartersMessage Relay::GenerateKeyQuartersMessage(MemoryDataStore &keydata)
{
    KeyQuartersMessage key_quarters_message;
    key_quarters_message.join_message_hash = join_message_hash;
    key_quarters_message.SetQuartersAndStoreCorrespondingSecrets(public_key, keydata);

    return key_quarters_message;
}

