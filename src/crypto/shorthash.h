#ifndef FLEX_SHORTHASH
#define FLEX_SHORTHASH

#define MAX_HASH_ENTRIES 30000

#include <vector>
#include "crypto/uint256.h"
#include "crypto/bignum.h"
#include "base/serialize.h"
#include "../../test/flex_tests/flex_data/TestData.h"


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
        HASH xor_of_full_hashes = 0ULL;
        for (uint32_t i = 0; i < full_hashes.size(); i++)
        {
            uint32_t short_hash;
            memcpy(UBEGIN(short_hash), UBEGIN(full_hashes[i]), 4);
            short_hashes.push_back(short_hash);
            xor_of_full_hashes ^= full_hashes[i];
        }
        disambiguator = xor_of_full_hashes;
    }

    bool RecoverFullHashes(MemoryDataStore& hashdata)
    {
        if (short_hashes.size() > MAX_HASH_ENTRIES)
            return false;

        std::vector<std::vector<HASH> > candidate_full_hashes;

        for (auto short_hash : short_hashes)
            candidate_full_hashes.push_back(hashdata[short_hash]["matches"]);

        std::vector<uint32_t> resulting_match(short_hashes.size());

        if (ThereIsACorrectMatch(candidate_full_hashes, resulting_match))
        {
            UseMatch(candidate_full_hashes, resulting_match);
            return true;
        }

        return false;
    }

    void UseMatch(std::vector<std::vector<HASH> > &candidate_full_hashes, std::vector<uint32_t> &match)
    {
        full_hashes.resize(0);
        for (uint64_t i = 0; i < short_hashes.size(); i++)
            full_hashes.push_back(candidate_full_hashes[i][match[i]]);
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

    bool ThereIsACorrectMatch(std::vector<std::vector<HASH> > &full_hashes,
                              std::vector<uint32_t> &resulting_match)
    {
        std::vector<uint32_t> working_hypothesis;

        return ThereIsACorrectMatch(full_hashes,
                                    working_hypothesis,
                                    resulting_match);
    }

    bool ThereIsACorrectMatch(std::vector<std::vector<HASH> > &full_hashes,
                              std::vector<uint32_t> &working_hypothesis,
                              std::vector<uint32_t> &resulting_match)
    {
        if (working_hypothesis.size() == resulting_match.size())
        {
            if (MultiXor(full_hashes, working_hypothesis) == disambiguator)
            {
                resulting_match = working_hypothesis;
                return true;
            }
            return false;
        }

        for (uint32_t individual_choice = 0; individual_choice < full_hashes[working_hypothesis.size()].size(); individual_choice++)
        {
            working_hypothesis.push_back(individual_choice);

            if (ThereIsACorrectMatch(full_hashes, working_hypothesis, resulting_match))
                return true;
        }
        return false;
    }
};

#endif
