#ifndef FLEX_NETWORKSEED_H
#define FLEX_NETWORKSEED_H


#include <src/crypto/uint256.h>

class NetworkMiningInfo
{
public:
    uint256 network_id;
    std::string network_ip;
    uint32_t network_port;
    uint256 network_seed;
    uint160 difficulty;

    NetworkMiningInfo() { }

    NetworkMiningInfo(uint256 network_id,
                      std::string network_ip,
                      uint32_t network_port,
                      uint256 network_seed,
                      uint160 difficulty):
            network_id(network_id),
            network_ip(network_ip),
            network_port(network_port),
            network_seed(network_seed),
            difficulty(difficulty)
    { }
};


#endif //FLEX_NETWORKSEED_H
