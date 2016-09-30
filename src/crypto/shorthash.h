#ifndef TELEPORT_SHORTHASH
#define TELEPORT_SHORTHASH

#define MAX_HASH_ENTRIES 30000
#define MAX_HASH_COMBINATIONS 10000000

#include "../../test/teleport_tests/teleport_data/TestData.h"
#include "src/base/util.h"
#include "src/crypto/hash.h"

#include "log.h"
#define LOG_CATEGORY "shorthash.h"

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

    std::vector<std::vector<HASH> > PossibleMatches(MemoryDataStore& hashdata)
    {
        std::vector<std::vector<HASH> > possible_matches;

        for (auto short_hash : short_hashes)
            possible_matches.push_back(hashdata[short_hash]["matches"]);

        return possible_matches;
    }

    bool RecoverFullHashes(MemoryDataStore& hashdata)
    {
        if (short_hashes.size() > MAX_HASH_ENTRIES)
        {
            log_ << "too many short hashes: " << short_hashes.size() << " vs " << MAX_HASH_ENTRIES << "\n";
            return false;
        }

        auto possible_matches = PossibleMatches(hashdata);

        for (uint32_t i = 0; i < short_hashes.size() ; i++)
            if (possible_matches[i].size() == 0)
                return false;

        if (NumberOfCombinations(possible_matches) > MAX_HASH_COMBINATIONS)
        {
            log_ << "too many combinations: "
                 << NumberOfCombinations(possible_matches) << " vs " << MAX_HASH_COMBINATIONS << "\n";
            return TryKnownSolution(hashdata);
        }

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

    bool TryKnownSolution(MemoryDataStore& hashdata)
    {
        uint160 this_list = this->GetHash160();
        if (not hashdata[this_list].HasProperty("known_solution"))
            return false;
        std::vector<HASH> known_solution = hashdata[this_list]["known_solution"];
        full_hashes = known_solution;
        return true;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(short_hashes);
        READWRITE(disambiguator);
    )

    JSON(short_hashes, disambiguator);

    HASH160();

    template <typename HASH2>
    bool operator==(const ShortHashList<HASH2> &other) const
    {
        return short_hashes == other.short_hashes and disambiguator == other.disambiguator;
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

    uint64_t NumberOfCombinations(std::vector<std::vector<HASH> >& possible_matches)
    {
        uint64_t number_of_combinations = 1;
        bool overflow = false;

        for (auto full_hashes_matching_individual_short_hash : possible_matches)
        {
            uint64_t number_of_matches = full_hashes_matching_individual_short_hash.size();
            if (number_of_matches == 0)
                return 0;
            uint64_t product = number_of_combinations * number_of_matches;
            if (product == 0 or product / number_of_combinations != number_of_matches)
                overflow = true;
            number_of_combinations = product;
        }
        if (overflow)
            return (uint64_t) -1;
        return number_of_combinations;
    }

    uint64_t NumberOfCombinations(MemoryDataStore& hashdata)
    {
        auto possible_matches = PossibleMatches(hashdata);
        return NumberOfCombinations(possible_matches);
    }
};



#endif
