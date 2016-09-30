#include <tgmath.h>
#include "DistributedSecret.h"

double DistributedSecret::probability_of_discovery(double fraction_conspiring)
{
    double probability = 0;
    for (auto number_of_parts : splittings)
        probability = combine_probabilities(probability, pow(fraction_conspiring, number_of_parts));

    probability = combine_probabilities(probability, 1 - pow(1 - fraction_conspiring, number_of_revelations));
    return probability;
}

void DistributedSecret::RevealToOneRelay()
{
    number_of_revelations += 1;
}

void DistributedSecret::SplitAmongRelays(uint32_t number_of_relays)
{
    splittings.push_back(number_of_relays);
}

double DistributedSecret::combine_probabilities(double probability1, double probability2)
{
    return probability1 + probability2 - probability1 * probability2;
}
