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

    bool RecoverFullHashes(MemoryDataStore& hashdata)
    {
        if (short_hashes.size() > MAX_HASH_ENTRIES)
            return false;
        full_hashes.resize(0);
        std::vector<std::vector<HASH> > full_hashes_;
        for (uint32_t i = 0; i < short_hashes.size(); i++)
        {
            std::vector<HASH> matches = hashdata[short_hashes[i]]["matches"];
            full_hashes_.push_back(matches);
        }

        uint64_t number_of_hashes = short_hashes.size();

        std::vector<uint32_t> resulting_matches;
        resulting_matches.resize(number_of_hashes);
        std::vector<uint32_t> working_hypothesis;
        bool successfully_got_result = TheresACorrectMatch(full_hashes_,
                                                           working_hypothesis,
                                                           resulting_matches, disambiguator);
        if (successfully_got_result)
            for (uint32_t i = 0; i < resulting_matches.size(); i++)
                full_hashes_.push_back(full_hashes_[i][resulting_matches[i]]);
        return successfully_got_result;
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

    bool TheresACorrectMatch(std::vector<std::vector<HASH> > &full_hashes,
                             std::vector<uint32_t> &working_hypothesis,
                             std::vector<uint32_t> &resulting_matches,
                             HASH disambiguator)
    {
        if (working_hypothesis.size() == resulting_matches.size())
        {
            if (MultiXor(full_hashes, working_hypothesis) == disambiguator)
            {
                resulting_matches = working_hypothesis;
                return true;
            }
            return false;
        }
        
        for (uint32_t individual_choice = 0; individual_choice < full_hashes[working_hypothesis.size()].size(); individual_choice++)
        {
            working_hypothesis.push_back(individual_choice);
            if (TheresACorrectMatch(full_hashes, working_hypothesis, resulting_matches, disambiguator))
                return true;
        }
        return false;
    }
};

#endif
