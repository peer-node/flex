#ifndef FLEX_ENCODEDNETWORKSTATE_H
#define FLEX_ENCODEDNETWORKSTATE_H

#include <src/crypto/uint256.h>
#include <src/base/serialize.h>
#include <src/base/version.h>
#include <src/crypto/hashtrees.h>

#ifndef INITIAL_DIFFICULTY
#define INITIAL_DIFFICULTY 10000000
#endif
#ifndef INITIAL_DIURNAL_DIFFICULTY
#define INITIAL_DIURNAL_DIFFICULTY 100000000
#endif

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
    uint160 previous_total_work{0};
    uint160 difficulty{0};
    uint160 diurnal_difficulty{0};
    uint160 previous_diurn_root{0};
    uint160 diurnal_block_root{0};
    uint64_t timestamp{0};


    IMPLEMENT_SERIALIZE
    (
        READWRITE(network_id);
        READWRITE(previous_mined_credit_hash);
        READWRITE(batch_number);
        READWRITE(batch_root);
        READWRITE(batch_offset);
        READWRITE(batch_size);
        READWRITE(message_list_hash);
        READWRITE(spent_chain_hash);
        READWRITE(previous_total_work);
        READWRITE(difficulty);
        READWRITE(previous_diurn_root);
        READWRITE(diurnal_block_root);
        READWRITE(diurnal_difficulty);
        READWRITE(timestamp);
    )

    JSON(network_id, previous_mined_credit_hash, batch_number, batch_root, batch_offset,
         batch_size, message_list_hash, spent_chain_hash, previous_total_work, difficulty,
         previous_diurn_root, diurnal_block_root, diurnal_difficulty, timestamp);

    bool operator==(const EncodedNetworkState& other) const
    {
        CDataStream stream1(SER_NETWORK, CLIENT_VERSION), stream2(SER_NETWORK, CLIENT_VERSION);
        stream1 << *this;
        stream2 << other;
        return stream1.str() == stream2.str();
    }

    uint160 DiurnRoot()
    {
        return SymmetricCombine(previous_diurn_root, diurnal_block_root);
    }
};


#endif //FLEX_ENCODEDNETWORKSTATE_H
