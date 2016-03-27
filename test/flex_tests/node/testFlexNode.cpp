#include <jsonrpccpp/client.h>

#define private public // to allow access to HttpClient.curl
#include <jsonrpccpp/client/connectors/httpclient.h>
#undef private

#include <fstream>
#include "gmock/gmock.h"
#include "FlexNode.h"


using namespace ::testing;
using namespace jsonrpc;
using namespace std;


class AFlexNode : public Test
{
public:
    FlexNode flexnode;
    string config_filename;
    int argc = 2;
    const char *argv[2] = {"", "-conf=flex_tmp_config_k5i4jn5u.conf"};

    virtual void SetUp()
    {
        config_filename = flexnode.config_parser.GetDataDir().string() + "/flex_tmp_config_k5i4jn5u.conf";
    }

    virtual void WriteConfigFile(string config)
    {
        ofstream stream(config_filename);
        stream << config << endl;
        stream.close();
        chmod(config_filename.c_str(), S_IRUSR | S_IWUSR);
    }

    virtual void DeleteConfigFile()
    {
        remove(config_filename.c_str());
    }

    virtual void TearDown()
    {
        DeleteConfigFile();
    }
};

TEST_F(AFlexNode, ReadsCommandLineArgumentsAndConfig)
{
    WriteConfigFile("a=b");
    flexnode.LoadConfig(argc, argv);
    ASSERT_THAT(flexnode.config["a"], Eq("b"));
}

TEST_F(AFlexNode, ThrowsAnExceptionIfNoRPCUsernameOrPasswordAreInTheConfigFile)
{
    WriteConfigFile("a=b");
    flexnode.LoadConfig(argc, argv);
    ASSERT_THROW(flexnode.StartRPCServer(), std::runtime_error);
}

class AFlexNodeWithRPCSettings : public AFlexNode
{
public:
    HttpClient *http_client;
    Client *client;

    virtual void SetUp()
    {
        AFlexNode::SetUp();
        WriteConfigFile("rpcuser=flexuser\n"
                        "rpcpassword=abcd123\n"
                        "rpcport=8383\n");
        flexnode.LoadConfig(argc, argv);
        flexnode.StartRPCServer();

        http_client = new HttpClient("http://localhost:8383");
        http_client->AddHeader("x", "y");
        client = new Client(*http_client);
    }

    virtual void TearDown()
    {
        AFlexNode::TearDown();
        delete client;
        delete http_client;
        flexnode.StopRPCServer();
    }
};

TEST_F(AFlexNodeWithRPCSettings, StartsAnRPCServer)
{
    Json::Value params;

    auto result = client->CallMethod("help", params);
    std::cout << result << endl;
    ASSERT_THAT(result.asString(), Ne(""));
}