
#ifndef FLEX_KNOWNHISTORYDECLARATION_H
#define FLEX_KNOWNHISTORYDECLARATION_H

#include <src/crypto/hashtrees.h>
#include "crypto/uint256.h"
#include "base/serialize.h"


class KnownHistoryDeclaration
{
public:
    BitChain known_diurns;
    BitChain known_diurn_message_data;
    std::vector<uint160> diurn_hashes;
    std::vector<uint160> diurn_message_data_hashes;

    KnownHistoryDeclaration() { }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(known_diurns);
        READWRITE(known_diurn_message_data);
        READWRITE(diurn_hashes);
        READWRITE(diurn_message_data_hashes);
    );

    JSON(known_diurns, known_diurn_message_data, diurn_hashes, diurn_message_data_hashes);
};


#endif //FLEX_KNOWNHISTORYDECLARATION_H
