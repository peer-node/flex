#include <fstream>
#include <src/base/util_time.h>
#include <boost/filesystem.hpp>
#include "gmock/gmock.h"
#include "test/teleport_tests/node/config/TeleportConfig.h"
#include "test/teleport_tests/node/config/ConfigParser.h"

using namespace ::testing;
using namespace std;


class AConfigParser : public Test
{
public:
    ConfigParser config_parser;
    string config_filename;

    virtual void SetUp()
    {
        config_filename = config_parser.GetDataDir().string() + "/teleport_tmp_config_k5i4jn5t.conf";
        int argc = 2;
        const char *argv[] = {"", "-conf=teleport_tmp_config_k5i4jn5t.conf"};
        config_parser.ParseCommandLineArguments(argc, argv);
    }

    virtual void WriteConfigFile(string config)
    {
        config_parser.CreateDataDirectoryIfItDoesntExist();
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

TEST_F(AConfigParser, CreatesTheDataDirectoryIfItDoesntExist)
{
    config_parser.config["datadir"] = "lq5ic7wsn3" + PrintToString(GetTimeMicros());
    ASSERT_FALSE(config_parser.Exists(config_parser.config.String("datadir")));
    config_parser.CreateDataDirectoryIfItDoesntExist();
    ASSERT_TRUE(config_parser.Exists(config_parser.config.String("datadir")));
    boost::filesystem::remove(config_parser.config.String("datadir"));
}

TEST_F(AConfigParser, ReadsAConfigFile)
{
    WriteConfigFile("a=b\n");
    config_parser.ReadConfigFile();
    ASSERT_THAT(config_parser.config["a"], Eq("b"));
}

TEST_F(AConfigParser, ThrowsAnExceptionIfTheConfigFileDoesntExist)
{
    ASSERT_THROW(config_parser.ReadConfigFile(), std::runtime_error);
}

TEST_F(AConfigParser, ThrowsAnExceptionIfTheConfigFileHasWrongPermissions)
{
    WriteConfigFile("a=b\n");
    chmod(config_filename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    ASSERT_THROW(config_parser.ReadConfigFile(), std::runtime_error);
}

TEST_F(AConfigParser, DoesntOverrideCommandLineArgumentsWithConfigFileContents)
{
    WriteConfigFile("conf=x");
    config_parser.ReadConfigFile();
    ASSERT_THAT(config_parser.config["conf"], Eq("teleport_tmp_config_k5i4jn5t.conf"));
}

TEST_F(AConfigParser, ReturnsAConfig)
{
    WriteConfigFile("a=b\n");
    config_parser.ReadConfigFile();
    TeleportConfig config = config_parser.GetConfig();
    ASSERT_THAT(config["a"], Eq("b"));
}