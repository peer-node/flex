#ifndef FLEX_SHORTHASH
#define FLEX_SHORTHASH

#define MAX_HASH_ENTRIES 30000

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
        HASH xor_of_full_hashes = 0;
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

        std::vector<std::vector<HASH> > possible_matches;

        for (auto short_hash : short_hashes)
            possible_matches.push_back(hashdata[short_hash]["matches"]);

        std::vector<uint32_t> resulting_match;

        if (ThereIsACorrectMatch(possible_matches, resulting_match))
        {
            UseMatch(possible_matches, resulting_match);
            return true;
        }
        return false;
    }

    void UseMatch(std::vector<std::vector<HASH> > &possible_matches, std::vector<uint32_t> &specific_matches)
    {
        full_hashes.resize(0);
        for (uint64_t i = 0; i < short_hashes.size(); i++)
            full_hashes.push_back(possible_matches[i][specific_matches[i]]);
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(short_hashes);
        READWRITE(disambiguator);
    )

    template <typename HASH2>
    bool operator==(const ShortHashList<HASH2> &other) const
    {
        return short_hashes == other.short_hashes and disambiguator == other.disambiguator;
    }

    uint160 GetHash160() const
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *this;
        return Hash160(ss.begin(), ss.end());
    }

    HASH MultiXor(std::vector<std::vector<HASH> > &hashes, std::vector<uint32_t> &choices)
    {
        HASH result = 0;

        for (uint32_t i = 0; i < hashes.size(); i++)
             result ^= hashes[i][choices[i]];

        return result;
    }

    bool ThereIsACorrectMatch(std::vector<std::vector<HASH> > &possible_matches,
                              std::vector<uint32_t> &resulting_match)
    {
        std::vector<uint32_t> working_hypothesis;

        return ThereIsACorrectMatch(possible_matches, working_hypothesis, resulting_match);
    }

    bool ThereIsACorrectMatch(std::vector<std::vector<HASH> > &possible_matches,
                              std::vector<uint32_t> &working_hypothesis,
                              std::vector<uint32_t> &resulting_match)
    {
        if (working_hypothesis.size() == possible_matches.size())
        {
            if (MultiXor(possible_matches, working_hypothesis) == disambiguator)
            {
                resulting_match = working_hypothesis;
                return true;
            }
            return false;
        }

        for (uint32_t individual_choice = 0; individual_choice < possible_matches[working_hypothesis.size()].size(); individual_choice++)
        {
            working_hypothesis.push_back(individual_choice);

            if (ThereIsACorrectMatch(possible_matches, working_hypothesis, resulting_match))
                return true;
        }
        return false;
    }
};

#endif
