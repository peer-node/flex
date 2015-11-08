#ifndef FLEX_SHORTHASH
#define FLEX_SHORTHASH

#define MAX_HASH_ENTRIES 30000

#include <vector>
#include "crypto/uint256.h"
#include "crypto/bignum.h"
#include "base/serialize.h"
#include "database/data.h"



template <typename HASH>
class ShortHashList
{
public:
    std::vector<HASH> full_hashes;
    std::vector<uint32_t> short_hashes;
    HASH disambiguator;

    void GenerateShortHashes()
    {
        short_hashes.resize(0);
        HASH xorhash = 0ULL;
        for (uint32_t i = 0; i < full_hashes.size(); i++)
        {
            uint32_t short_hash;
            memcpy(UBEGIN(short_hash), UBEGIN(full_hashes[i]), 4);
            short_hashes.push_back(short_hash);
            xorhash ^= full_hashes[i];
        }
        disambiguator = xorhash;
    }

    bool RecoverFullHashes()
    {
        if (short_hashes.size() > MAX_HASH_ENTRIES)
            return false;
        full_hashes.resize(0);
        std::vector<std::vector<HASH> > candidates;
        for (uint32_t i = 0; i < short_hashes.size(); i++)
        {
            std::vector<HASH> matches = hashdata[short_hashes[i]]["matches"];
            candidates.push_back(matches);
        }

        std::vector<uint32_t> choices;
        choices.resize(short_hashes.size());
        std::vector<uint32_t> fixed_choices;
        bool result = FindMatchingChoices(candidates, 
                                          fixed_choices,
                                          choices, disambiguator);
        if (result)
            for (uint32_t i = 0; i < choices.size(); i++)
                full_hashes.push_back(candidates[i][choices[i]]);
        return result;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(short_hashes);
        READWRITE(disambiguator);
    )

    uint160 GetHash160() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    HASH MultiXor(std::vector<std::vector<HASH> > &hashes,
                  std::vector<uint32_t> &choices)
    {
        HASH result = 0ULL;

        for (uint32_t i = 0; i < hashes.size(); i++)
             result ^= hashes[i][choices[i]];

        return result;
    }

    bool FindMatchingChoices(std::vector<std::vector<HASH> > &hashes,
                             std::vector<uint32_t> &fixed_choices,
                             std::vector<uint32_t> &choices,
                             HASH disambiguator)
    {
        if (fixed_choices.size() == choices.size())
        {
            if (MultiXor(hashes, fixed_choices) == disambiguator)
            {
                choices = fixed_choices;
                return true;
            }
            return false;
        }
        
        for (uint32_t j = 0; j < hashes[fixed_choices.size()].size(); j++)
        {
            fixed_choices.push_back(j);
            if (FindMatchingChoices(hashes, fixed_choices, 
                                    choices, disambiguator))
                return true;
        }
        return false;
    }
};

#endif
