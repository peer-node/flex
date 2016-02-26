#include "gmock/gmock.h"
#include "mining/work.h"
#include "FlexMiner.h"
#include "MockRPCConnection.h"

using namespace ::testing;


TEST(AnRPCConnection, CanHaveRequestsSentToIt)
{
    RPCConnection connection;
    std::string method("test_method");
    json_spirit::Array params;
    json_spirit::Value result = connection.SendRPCRequest(method, params);
}


TEST(AMockRPCConnection, RecordsRequestsSentToIt)
{
    MockRPCConnection connection;

    std::string method("test_method");
    json_spirit::Array params;
    json_spirit::Value result = connection.SendRPCRequest(method, params);

    ASSERT_THAT(connection.requests.size(), Eq(1));
    ASSERT_THAT(connection.requests[0].first, Eq("test_method"));
}


class AFlexMiner : public Test
{
public:
    FlexMiner miner;
    NetworkMiningInfo network_mining_info;

    virtual void SetUp()
    {
        uint32_t network_port = 123;
        std::string network_ip("123.456.789.12");
        uint256 network_seed(123456);
        uint256 network_id(1);
        uint160 difficulty(10000);

        network_mining_info = NetworkMiningInfo(network_id, network_ip, network_port, network_seed, difficulty);
    }
};

TEST_F(AFlexMiner, IsNotInitiallyMining)
{
    ASSERT_FALSE(miner.IsMining());
}

TEST_F(AFlexMiner, InitiallyHasNoMiningInfo)
{
    ASSERT_THAT(miner.network_id_to_mining_info.size(), Eq(0));
}

TEST_F(AFlexMiner, HasMiningInfoWhenOneIsAdded)
{
    miner.AddNetworkMiningInfo(network_mining_info);
    ASSERT_THAT(miner.network_id_to_mining_info.size(), Eq(1));
}

TEST_F(AFlexMiner, CanAddAnRPCConnection)
{
    RPCConnection connection;
    miner.AddRPCConnection(&connection);
    ASSERT_THAT(miner.network_id_to_network_connection.size(), Eq(1));
}

TEST_F(AFlexMiner, CanAddAMockRPCConnection)
{
    MockRPCConnection connection;
    miner.AddRPCConnection(&connection);
    ASSERT_THAT(miner.network_id_to_network_connection.size(), Eq(1));
}


class AFlexMinerWithAMockRPCConnection : public AFlexMiner
{
public:
    MockRPCConnection connection;

    virtual void SetUp()
    {
        AFlexMiner::SetUp();
        miner.AddRPCConnection(&connection);
    }
};

TEST_F(AFlexMinerWithAMockRPCConnection, StoresARequest)
{
    std::string method("test_method");
    json_spirit::Array params;

    ASSERT_THAT(miner.network_id_to_network_connection.size(), Eq(1));
    ASSERT_TRUE(miner.network_id_to_network_connection.count(0));

    json_spirit::Value result = miner.network_id_to_network_connection[0]->SendRPCRequest(method, params);
    uint64_t number_of_requests = miner.network_id_to_network_connection[0]->NumberOfRequests();

    ASSERT_THAT(number_of_requests, Eq(1));
}


class AMiningHashTree : public Test
{
public:
    MiningHashTree tree;
    uint256 hash1;
    uint256 hash2;

    virtual void SetUp()
    {
        hash1 = 1;
        hash2 = 2;
    }
};

TEST_F(AMiningHashTree, HasNoHashesInitially)
{
    uint64_t number_of_hashes = tree.hashes.size();
    ASSERT_THAT(number_of_hashes, Eq(0));
}

TEST_F(AMiningHashTree, HasZeroRootInitially)
{
    uint256 root = tree.Root();
    ASSERT_THAT(root, Eq(0));
}

TEST_F(AMiningHashTree, CombinesHashesSymmetrically)
{
    uint256 combination1 = MiningHashTree::Combine(hash1, hash2);
    uint256 combination2 = MiningHashTree::Combine(hash2, hash1);
    ASSERT_THAT(combination1, Eq(combination2));
}

TEST_F(AMiningHashTree, GivesNonTrivialHashesForCombinations)
{
    uint256 combination = MiningHashTree::Combine(hash1, hash2);
    ASSERT_THAT(combination, Ne(0));
}

TEST_F(AMiningHashTree, DoesntGiveSameResultForInputsWithSameXor)
{
    hash1 = 1;
    hash2 = 2;
    uint256 hash1_xor_hash2 = hash1 ^ hash2;

    uint256 hash3 = 5;
    uint256 hash4 = 6;
    uint256 hash3_xor_hash4 = hash3 ^ hash4;

    ASSERT_THAT(hash1_xor_hash2, Eq(hash3_xor_hash4));

    uint256 combination1 = MiningHashTree::Combine(hash1, hash2);
    uint256 combination2 = MiningHashTree::Combine(hash3, hash4);

    ASSERT_THAT(combination1, Ne(combination2));
}

class AMiningHashTreeWithAHash : public AMiningHashTree
{
public:
    virtual void SetUp()
    {
        AMiningHashTree::SetUp();
        tree.AddHash(hash1);
    }
};

TEST_F(AMiningHashTreeWithAHash, HasARootEqualToTheHash)
{
    uint256 root = tree.Root();
    ASSERT_THAT(root, Eq(hash1));
}

TEST_F(AMiningHashTreeWithAHash, ProvidesAValidBranchForTheHash)
{
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash1)), Eq(tree.Root()));
}

class AMiningHashTreeWithTwoHashes : public AMiningHashTree
{
public:
    virtual void SetUp()
    {
        AMiningHashTree::SetUp();
        tree.AddHash(hash1);
        tree.AddHash(hash2);
    }
};

TEST_F(AMiningHashTreeWithTwoHashes, HasARootEqualToTheCombinationOfTheHashes)
{
    uint256 root = tree.Root();
    uint256 combination = MiningHashTree::Combine(hash1, hash2);
    ASSERT_THAT(root, Eq(combination));
}

class AMiningHashTreeWithThreeHashes : public AMiningHashTree
{
public:
    uint256 hash3;

    virtual void SetUp()
    {
        hash3 = 3;

        AMiningHashTree::SetUp();
        tree.AddHash(hash1);
        tree.AddHash(hash2);
        tree.AddHash(hash3);
    }
};

TEST_F(AMiningHashTreeWithThreeHashes, CombinesTheThirdWithTheCombinationOfTheFirstTwo)
{
    uint256 root = tree.Root();
    uint256 combination_of_first_two = MiningHashTree::Combine(hash1, hash2);
    uint256 final_combination = MiningHashTree::Combine(hash3, combination_of_first_two);
    ASSERT_THAT(root, Eq(final_combination));
}

TEST_F(AMiningHashTreeWithThreeHashes, ProvidesAValidBranchForAGivenHash)
{
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash1)), Eq(tree.Root()));
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash2)), Eq(tree.Root()));
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash3)), Eq(tree.Root()));
}


class AMiningHashTreeWithFourHashes : public AMiningHashTree
{
public:
    uint256 hash3;
    uint256 hash4;

    virtual void SetUp()
    {
        hash3 = 3;
        hash4 = 4;

        AMiningHashTree::SetUp();
        tree.AddHash(hash1);
        tree.AddHash(hash2);
        tree.AddHash(hash3);
        tree.AddHash(hash4);
    }
};

TEST_F(AMiningHashTreeWithFourHashes, ProvidesAValidBranchForAGivenHash)
{
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash1)), Eq(tree.Root()));
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash2)), Eq(tree.Root()));
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash3)), Eq(tree.Root()));
    ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(hash4)), Eq(tree.Root()));
}

TEST(AMiningHashTreeWithManyHashes, ProvidesAValidBranchForAGivenHash)
{
    MiningHashTree tree;
    uint32_t number_of_hashes = 117;

    for (uint32_t i = 0; i < number_of_hashes; i++)
        tree.AddHash(i);

    for (uint32_t i = 0; i < number_of_hashes; i++)
        ASSERT_THAT(MiningHashTree::EvaluateBranch(tree.Branch(i)), Eq(tree.Root()));
}


class AFlexMinerWithMiningInfo : public AFlexMiner
{
public:
    virtual void SetUp()
    {
        AFlexMiner::SetUp();
        miner.AddNetworkMiningInfo(network_mining_info);
    }
};

TEST_F(AFlexMinerWithMiningInfo, ReturnsAValidBranchForTheMiningInfo)
{
    std::vector<uint256> branch = miner.BranchForNetwork(network_mining_info.network_id);
    ASSERT_THAT(branch[0], Eq(network_mining_info.network_seed));
    ASSERT_THAT(MiningHashTree::EvaluateBranch(branch), Eq(miner.MiningRoot()));
}

TEST_F(AFlexMinerWithMiningInfo, CompletesAProofOfWorkUsingTheMiningRoot)
{
    miner.SetMemoryUsageInMegabytes(1);
    miner.StartMining();
    TwistWorkProof proof = miner.GetProof();
    ASSERT_THAT(proof.memoryseed, Eq(miner.MiningRoot()));
    ASSERT_THAT(proof.DifficultyAchieved(), Gt(0));
}
