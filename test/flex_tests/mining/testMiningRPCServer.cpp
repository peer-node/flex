#include "gmock/gmock.h"
#include "MiningRPCServer.h"
#include "NetworkSpecificProofOfWork.h"

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AMiningRPCServer : public Test
{
public:
    HttpServer *http_server;
    MiningRPCServer *server;
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        http_server = new HttpServer(8383);
        server = new MiningRPCServer(*http_server);
        server->StartListening();
        http_client = new HttpClient("http://localhost:8383");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        delete client;
        delete http_client;
        server->StopListening();
        delete server;
        delete http_server;
    }
};

TEST_F(AMiningRPCServer, ProvidesItsVersion)
{
    Json::Value params;
    auto result = client->CallMethod("version", params);

    ASSERT_STREQ(result.asCString(), FLEX_MINER_VERSION);
}

TEST_F(AMiningRPCServer, ListsSeeds)
{
    Json::Value params;
    auto result = client->CallMethod("list_seeds", params);

    ASSERT_TRUE(result.isArray());
    ASSERT_THAT(result.size(), Eq(0));
}

TEST_F(AMiningRPCServer, SetsAndGetsTheMemoryUsage)
{
    Json::Value params;

    params["megabytes_used"] = 8;

    client->CallMethod("set_megabytes_used", params);

    auto result = client->CallMethod("get_megabytes_used", params);

    ASSERT_THAT(result.asInt64(), Eq(8));
}

class AMiningRPCServerWithMiningInformation : public AMiningRPCServer
{
public:
    Json::Value params;

    virtual void SetUp()
    {
        AMiningRPCServer::SetUp();

        params["network_id"] = uint256(1).ToString();
        params["network_host"] = "localhost";
        params["network_port"] = 8384;
        params["network_seed"] = uint256(2).ToString();
        params["network_difficulty"] = 100;
    }
};

TEST_F(AMiningRPCServerWithMiningInformation, AcceptsTheMiningInformation)
{
    auto result = client->CallMethod("set_mining_information", params);

    ASSERT_STREQ(result.asCString(), "ok");
}

TEST_F(AMiningRPCServerWithMiningInformation, ReturnsItInTheListOfSeeds)
{
    client->CallMethod("set_mining_information", params);

    auto result = client->CallMethod("list_seeds", params);

    ASSERT_THAT(result.size(), Eq(1));
    ASSERT_THAT(uint256(result[0]["network_id"].asString()), Eq(1));
    ASSERT_THAT(uint256(result[0]["network_seed"].asString()), Eq(2));
}

TEST_F(AMiningRPCServerWithMiningInformation, ProvidesAMiningRoot)
{
    client->CallMethod("set_mining_information", params);

    auto result = client->CallMethod("get_mining_root", params);
    ASSERT_STREQ(result.asCString(), params["network_seed"].asCString());
}

TEST_F(AMiningRPCServerWithMiningInformation, MinesAProofOfWork)
{
    client->CallMethod("set_mining_information", params);

    params["megabytes_used"] = 8;

    client->CallMethod("set_megabytes_used", params);

    client->CallMethod("start_mining", params);

    auto result = client->CallMethod("get_proof_of_work", params);

    std::string proof_base64 = result["proof_base64"].asString();

    NetworkSpecificProofOfWork proof(proof_base64);

    ASSERT_TRUE(proof.IsValid());
}