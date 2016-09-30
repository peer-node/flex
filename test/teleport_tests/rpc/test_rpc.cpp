#include "gmock/gmock.h"

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>

using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class TestServer : public AbstractServer<TestServer>
{
public:
    TestServer(HttpServer &server) :
            AbstractServer<TestServer>(server)
    {
        this->bindAndAddMethod(Procedure("sayHello", PARAMS_BY_NAME, JSON_STRING, "name", JSON_STRING, NULL),
                               &TestServer::sayHello);
    }

    void sayHello(const Json::Value& request, Json::Value& response)
    {
        response = "Hello: " + request["name"].asString();
    }
};


TEST(AnRpcConnection, HandlesRequestsCorrectly)
{
    HttpServer http_server(8383);
    TestServer server(http_server);
    ASSERT_TRUE(server.StartListening());

    HttpClient http_client("http://localhost:8383");
    Client client(http_client);

    Json::Value params;
    params["name"] = "Peter";

    stringstream output;
    output << client.CallMethod("sayHello", params);

    ASSERT_THAT(output.str(), Eq("\"Hello: Peter\"\n"));

    server.StopListening();
}
