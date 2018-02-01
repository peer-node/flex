#ifndef TELEPORT_MINEDCREDIT_H
#define TELEPORT_MINEDCREDIT_H

#include <src/crypto/point.h>
#include "credits/Credit.h"
#include "EncodedNetworkState.h"


class MinedCredit : public Credit
{
public:
    EncodedNetworkState network_state;

    static std::string Type() { return "mined_credit"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(amount);
        READWRITE(public_key);
        READWRITE(network_state);
    )

    JSON(amount, public_key, network_state);

    uint160 BranchBridge()
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    uint160 GetHash160()
    {
        return SymmetricCombine(BranchBridge(), network_state.batch_root);
    }

    uint256 GetHash()
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    uint160 ReportedWork()
    {
        return network_state.previous_total_work + network_state.difficulty;
    }

    Point PublicKey()
    {
        return public_key;
    }

    bool operator==(const MinedCredit& other) const
    {
        return amount == other.amount and public_key == other.public_key and network_state == other.network_state;
    }

    bool operator!=(const MinedCredit& other) const
    {
        return not (*this == other);
    }
};


#endif //TELEPORT_MINEDCREDIT_H
