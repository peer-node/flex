#include <src/crypto/uint256.h>
#include <src/vector_tools.h>
#include "gmock/gmock.h"
#include "src/crypto/shorthash.h"

using namespace ::testing;
using namespace std;

class AShortHashList : public Test
{
public:
    MemoryDataStore msgdata;
    ShortHashList<uint160> hash_list;

    void StoreHash(uint160 hash)
    {
        uint32_t short_hash;
        memcpy(UBEGIN(short_hash), UBEGIN(hash), 4);
        vector<uint160> matches = msgdata[short_hash]["matches"];
        if (not VectorContainsEntry(matches, hash))
            matches.push_back(hash);
        msgdata[short_hash]["matches"] = matches;
    }

    virtual void SetUp()
    {
        StoreHash(1);
        StoreHash(2);
        hash_list.full_hashes.push_back(1);
        hash_list.full_hashes.push_back(2);
    }

};

TEST_F(AShortHashList, GeneratesShortHashes)
{
    hash_list.GenerateShortHashes();
    ASSERT_THAT(hash_list.short_hashes[0], Eq(1));
    ASSERT_THAT(hash_list.short_hashes[1], Eq(2));
}

TEST_F(AShortHashList, RecoversFullHashes)
{
    hash_list.GenerateShortHashes();
    ASSERT_TRUE(hash_list.RecoverFullHashes(msgdata));
    ASSERT_THAT(hash_list.full_hashes[0], Eq(1));
    ASSERT_THAT(hash_list.full_hashes[1], Eq(2));
}


class AShortHashListWithCollisions : public AShortHashList
{
public:
    virtual void SetUp()
    {
        AShortHashList::SetUp();
        for (uint32_t i = 0; i < 50; i++)
        {
            StoreHash(i);
            StoreHash(i + (1ULL << 35));
        }
    }
};

TEST_F(AShortHashListWithCollisions, RecoversFullHashes)
{
    hash_list.GenerateShortHashes();
    ASSERT_TRUE(hash_list.RecoverFullHashes(msgdata));
    ASSERT_THAT(hash_list.full_hashes[0], Eq(1));
    ASSERT_THAT(hash_list.full_hashes[1], Eq(2));
}

TEST_F(AShortHashListWithCollisions, RecoversFullHashesInTheCorrectOrder)
{
    StoreHash(60);
    StoreHash(60 + (1ULL << 35));

    hash_list.full_hashes.push_back(60 + (1ULL << 35));
    hash_list.full_hashes.push_back(60);

    hash_list.GenerateShortHashes();
    hash_list.full_hashes.resize(0);

    ASSERT_TRUE(hash_list.RecoverFullHashes(msgdata));
    ASSERT_THAT(hash_list.full_hashes[0], Eq(1));
    ASSERT_THAT(hash_list.full_hashes[1], Eq(2));
    ASSERT_THAT(hash_list.full_hashes[2], Eq(60 + (1ULL << 35)));
    ASSERT_THAT(hash_list.full_hashes[3], Eq(60));
}

TEST_F(AShortHashListWithCollisions, DeterminesTheNumberOfCombinationsToTry)
{
    hash_list.GenerateShortHashes();
    uint64_t number_of_combinations = hash_list.NumberOfCombinations(msgdata);
    ASSERT_THAT(number_of_combinations, Eq(4));
}

TEST_F(AShortHashListWithCollisions, FailsIfThereAreTooManyCombinationsToTry)
{
    hash_list.full_hashes.resize(0);
    for (uint32_t i = 0; i < 30; i ++)
        hash_list.full_hashes.push_back(i);
    hash_list.GenerateShortHashes();
    uint64_t number_of_combinations = hash_list.NumberOfCombinations(msgdata);
    ASSERT_THAT(number_of_combinations, Gt(MAX_HASH_COMBINATIONS));
    ASSERT_FALSE(hash_list.RecoverFullHashes(msgdata));
}

TEST_F(AShortHashListWithCollisions, SucceedsIfThereAreTooManyCombinationsToTryButTheresAKnownSolution)
{
    hash_list.full_hashes.resize(0);
    for (uint32_t i = 0; i < 30; i ++)
        hash_list.full_hashes.push_back(i);
    hash_list.GenerateShortHashes();

    msgdata[hash_list.GetHash160()]["known_solution"] = hash_list.full_hashes;

    uint64_t number_of_combinations = hash_list.NumberOfCombinations(msgdata);
    ASSERT_THAT(number_of_combinations, Gt(MAX_HASH_COMBINATIONS));
    ASSERT_TRUE(hash_list.RecoverFullHashes(msgdata));
}
