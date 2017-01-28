#ifndef TELEPORT_RELAYEXIT_H
#define TELEPORT_RELAYEXIT_H


#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/define.h>


class RelayExit
{
public:
    uint160 obituary_hash;
    std::vector<uint160> secret_recovery_message_hashes;

    static std::string Type() { return "relay_exit"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(obituary_hash);
        READWRITE(secret_recovery_message_hashes);
    );

    JSON(obituary_hash, secret_recovery_message_hashes);

    DEPENDENCIES();

    HASH160();
};


#endif //TELEPORT_RELAYEXIT_H
