#ifndef FLEX_ENCODEDNETWORKSTATE_H
#define FLEX_ENCODEDNETWORKSTATE_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>


class EncodedNetworkState
{
public:
    uint256 network_id{0};
    uint160 previous_mined_credit_hash{0};
    uint32_t batch_number{0};
    uint160 batch_root{0};
    uint32_t batch_offset{0};
    uint32_t batch_size{0};
    uint160 message_list_hash{0};
    uint160 spent_chain_hash{0};


    IMPLEMENT_SERIALIZE
    (
        READWRITE(network_id);
        READWRITE(previous_mined_credit_hash);
        READWRITE(batch_number);
        READWRITE(batch_root);
        READWRITE(batch_offset);
        READWRITE(batch_size);
        READWRITE(message_list_hash);
    )

};


#endif //FLEX_ENCODEDNETWORKSTATE_H
