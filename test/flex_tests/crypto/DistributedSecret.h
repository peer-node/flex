#ifndef FLEX_DISTRIBUTEDSECRET_H
#define FLEX_DISTRIBUTEDSECRET_H


#include <stdint.h>
#include <vector>

class DistributedSecret
{
public:
    uint32_t number_of_revelations;
    std::vector<uint32_t> splittings;

    DistributedSecret(): number_of_revelations(0) { }

    double probability_of_discovery(double fraction_conspiring);

    void RevealToOneRelay();

    void SplitAmongRelays(uint32_t number_of_relays);


    double combine_probabilities(double probability, double pow);
};


#endif //FLEX_DISTRIBUTEDSECRET_H
