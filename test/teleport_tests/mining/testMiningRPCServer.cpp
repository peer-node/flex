#include "gmock/gmock.h"
#include "MiningRPCServer.h"
#include "server_key.h"
#include "NetworkSpecificProofOfWork.h"

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private

#include <fstream>
#include <test/teleport_tests/node/server/HttpAuthServer.h>

using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AMiningRPCServer : public Test
{
public:
    HttpAuthServer *http_server;
    MiningRPCServer *server;
    HttpClient *http_client;
    Client *client;
    Json::Value params;

    virtual void SetUp()
    {
        http_server = new HttpAuthServer(8383, "username", "password");
        server = new MiningRPCServer(*http_server);
        server->StartListening();
        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("Authorization", "Basic username:password");
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
    auto result = client->CallMethod("version", params);
    ASSERT_STREQ(result.asCString(), TELEPORT_MINER_VERSION);
}

TEST_F(AMiningRPCServer, ListsSeeds)
{
    auto result = client->CallMethod("list_seeds", params);
    ASSERT_TRUE(result.isArray());
    ASSERT_THAT(result.size(), Eq(0));
}

TEST_F(AMiningRPCServer, SetsAndGetsTheMemoryUsage)
{
    params["megabytes_used"] = 8;
    client->CallMethod("set_megabytes_used", params);
    auto result = client->CallMethod("get_megabytes_used", params);
    ASSERT_THAT(result.asInt64(), Eq(8));
}


class AMiningRPCServerWithMiningInformation : public AMiningRPCServer
{
public:
    Json::Value params;
    Json::Value result;

    virtual void SetUp()
    {
        AMiningRPCServer::SetUp();

        params["network_id"] = uint256(1).ToString();
        params["network_host"] = "localhost";
        params["network_port"] = 0;
        params["network_seed"] = uint256(2).ToString();
        params["network_difficulty"] = uint160(100).ToString();

        result = client->CallMethod("set_mining_information", params);
    }
};

TEST_F(AMiningRPCServerWithMiningInformation, AcceptsTheMiningInformation)
{
    ASSERT_STREQ(result.asCString(), "ok");
}

TEST_F(AMiningRPCServerWithMiningInformation, ReturnsItInTheListOfSeeds)
{
    result = client->CallMethod("list_seeds", params);

    ASSERT_THAT(result.size(), Eq(1));
    ASSERT_THAT(uint256(result[0]["network_id"].asString()), Eq(1));
    ASSERT_THAT(uint256(result[0]["network_seed"].asString()), Eq(2));
}

TEST_F(AMiningRPCServerWithMiningInformation, ProvidesAMiningRoot)
{
    result = client->CallMethod("get_mining_root", params);
    ASSERT_STREQ(result.asCString(), params["network_seed"].asCString());
}

TEST_F(AMiningRPCServerWithMiningInformation, MinesAProofOfWork)
{
    params["megabytes_used"] = 8;
    client->CallMethod("set_megabytes_used", params);
    client->CallMethod("start_mining", params);

    result = client->CallMethod("get_proof_of_work", params);
    NetworkSpecificProofOfWork proof(result["proof_base64"].asString());
    ASSERT_TRUE(proof.IsValid());
}


class TestRPCProofListener : public AbstractServer<TestRPCProofListener>
{
public:
    bool received_proof = false;
    string proof_base64;

    TestRPCProofListener(HttpServer &server) :
            AbstractServer<TestRPCProofListener>(server)
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

    void WaitForProof()
    {
        while (not received_proof)
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
};


class AMiningRPCServerWithAListener : public AMiningRPCServerWithMiningInformation
{
public:
    HttpServer *http_server;
    TestRPCProofListener *listener;

    virtual void SetUp()
    {
        AMiningRPCServerWithMiningInformation::SetUp();
        listener = GetListener();
        listener->StartListening();

        params["network_port"] = 8387;
        result = client->CallMethod("set_mining_information", params);

        params["megabytes_used"] = 8;
        result = client->CallMethod("set_megabytes_used", params);
    }

    virtual TestRPCProofListener *GetListener()
    {
        http_server = new HttpServer(8387);
        return new TestRPCProofListener(*http_server);
    }

    virtual void TearDown()
    {
        listener->StopListening();
        delete listener;
        delete http_server;
    }
};

TEST_F(AMiningRPCServerWithAListener, MakesAnRPCCallAfterEachProof)
{
    ASSERT_FALSE(listener->received_proof);
    client->CallMethod("start_mining", params);
    ASSERT_TRUE(listener->received_proof);
}

TEST_F(AMiningRPCServerWithAListener, SendsAValidNetworkSpecificProofOfWorkWithTheRPCCall)
{
    client->CallMethod("start_mining", params);
    NetworkSpecificProofOfWork proof(listener->proof_base64);
    ASSERT_TRUE(proof.IsValid());
}

TEST_F(AMiningRPCServerWithAListener, MinesSynchronously)
{
    client->CallMethod("start_mining", params);
    ASSERT_TRUE(listener->received_proof);
    listener->WaitForProof();
    NetworkSpecificProofOfWork proof(listener->proof_base64);
    ASSERT_TRUE(proof.IsValid());
}

TEST_F(AMiningRPCServerWithAListener, MinesAsynchronously)
{
    client->CallMethod("start_mining_asynchronously", params);
    ASSERT_FALSE(listener->received_proof);
    listener->WaitForProof();
    NetworkSpecificProofOfWork proof(listener->proof_base64);
    ASSERT_TRUE(proof.IsValid());
}


class AMiningRPCServerWithSSL : public AMiningRPCServer
{
public:
    string certificate_file;
    string server_key_file;

    virtual void SetUp()
    {
        CreateFiles();
        http_server = new HttpAuthServer(8389, "username", "password", certificate_file, server_key_file);
        server = new MiningRPCServer(*http_server);
        server->StartListening();
        http_client = new HttpClient("https://localhost:8389");
        curl_easy_setopt(http_client->curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(http_client->curl, CURLOPT_SSL_VERIFYHOST, 0);
        http_client->AddHeader("Authorization", "Basic username:password");
        client = new Client(*http_client);
    }

    void WriteStringToFile(string data, string filename)
    {
        ofstream stream(filename);
        stream << data;
        stream.close();
    }

    void CreateFiles()
    {
        server_key_file = "tmp_server_key_Wj3Hhp.key";
        WriteStringToFile(sample_server_key, server_key_file);
        certificate_file = "tmp_server_cert_Wj3Hhp.pem";
        WriteStringToFile(sample_server_cert, certificate_file);
    }

    void DeleteFiles()
    {
        remove(server_key_file.c_str());
        remove(certificate_file.c_str());
    }

    virtual void TearDown()
    {
        delete client;
        delete http_client;
        server->StopListening();
        delete server;
        delete http_server;
        DeleteFiles();
    }
};

TEST_F(AMiningRPCServerWithSSL, ProvidesItsVersion)
{
    auto result = client->CallMethod("version", params);
    ASSERT_STREQ(result.asCString(), TELEPORT_MINER_VERSION);
}
