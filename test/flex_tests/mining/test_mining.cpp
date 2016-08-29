#include "gmock/gmock.h"
#include "mining/work.h"
#include "FlexMiner.h"
#include "NetworkSpecificProofOfWork.h"

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AFlexMiner : public Test
{
public:
    FlexMiner miner;
};

TEST_F(AFlexMiner, IsNotInitiallyMining)
{
    ASSERT_FALSE(miner.IsMining());
}

TEST_F(AFlexMiner, InitiallyHasNoMiningInfo)
{
    ASSERT_THAT(miner.network_id_to_mining_info.size(), Eq(0));
}


class AFlexMinerWithMiningInfo : public AFlexMiner
{
public:
    NetworkMiningInfo network_mining_info;

    virtual void SetUp()
    {
        AFlexMiner::SetUp();
        uint32_t network_port = 0;
        std::string network_host("localhost");
        uint256 network_seed(123456);
        uint64_t network_id = 1;
        uint160 difficulty(10000);

        network_mining_info = NetworkMiningInfo(network_id, network_host, network_port, network_seed, difficulty);

        miner.AddNetworkMiningInfo(network_mining_info);
    }
};

TEST_F(AFlexMinerWithMiningInfo, HasMiningInfo)
{
    ASSERT_THAT(miner.network_id_to_mining_info.size(), Eq(1));
}

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
    ASSERT_THAT(proof.memory_seed, Eq(miner.MiningRoot()));
    ASSERT_THAT(proof.DifficultyAchieved(), Gt(0));
}


class TestRPCProofListener : public jsonrpc::AbstractServer<TestRPCProofListener>
{
public:
    bool received_proof = false;
    string proof_base64;

    TestRPCProofListener(jsonrpc::HttpServer &server) :
            jsonrpc::AbstractServer<TestRPCProofListener>(server)
    {
        BindMethod("new_proof", &TestRPCProofListener::NewProof);
    }

    void NewProof(const Json::Value& request, Json::Value& response)
    {
        proof_base64 = request["proof_base64"].asString();
        received_proof = true;
    }

    void BindMethod(const char* method_name,
                    void (TestRPCProofListener::*method)(const Json::Value &,Json::Value &))
    {
        this->bindAndAddMethod(Procedure(method_name, PARAMS_BY_NAME, JSON_STRING, NULL), method);
    }
};


class AFlexMinerWithAProofListener : public AFlexMinerWithMiningInfo
{
public:
    HttpServer *http_server;
    TestRPCProofListener *listener;

    virtual void SetUp()
    {
        AFlexMinerWithMiningInfo::SetUp();
        http_server = new HttpServer(8387);
        listener = new TestRPCProofListener(*http_server);
        listener->StartListening();

        network_mining_info.network_port = 8387;
        miner.AddNetworkMiningInfo(network_mining_info);
        miner.SetMemoryUsageInMegabytes(8);
    }

    virtual void TearDown()
    {
        listener->StopListening();
        delete listener;
        delete http_server;
    }
};

TEST_F(AFlexMinerWithAProofListener, MakesAnRPCCallAfterEachProofIfPortIsNonZero)
{
    ASSERT_FALSE(listener->received_proof);
    miner.StartMining();
    ASSERT_TRUE(listener->received_proof);
}

TEST_F(AFlexMinerWithAProofListener, SendsAValidNetworkSpecificProofOfWorkWithTheRPCCall)
{
    miner.StartMining();
    NetworkSpecificProofOfWork proof(listener->proof_base64);
    ASSERT_TRUE(proof.IsValid());
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

