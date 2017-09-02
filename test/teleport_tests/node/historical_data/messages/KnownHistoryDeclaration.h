
#ifndef TELEPORT_KNOWNHISTORYDECLARATION_H
#define TELEPORT_KNOWNHISTORYDECLARATION_H

#include <src/crypto/hashtrees.h>
#include "crypto/uint256.h"
#include "base/serialize.h"


class KnownHistoryDeclaration
{
public:
    BitChain known_diurns;
    std::vector<uint160> diurn_hashes;
    std::vector<uint160> diurn_message_data_hashes;

    KnownHistoryDeclaration() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(known_diurns);
        READWRITE(diurn_hashes);
        READWRITE(diurn_message_data_hashes);
    );

    JSON(known_diurns, diurn_hashes, diurn_message_data_hashes);
};


#endif //TELEPORT_KNOWNHISTORYDECLARATION_H
