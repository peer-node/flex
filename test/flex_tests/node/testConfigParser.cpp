#include <fstream>
#include "gmock/gmock.h"
#include "FlexConfig.h"
#include "ConfigParser.h"

using namespace ::testing;
using namespace std;


class AConfigParser : public Test
{
public:
    ConfigParser config_parser;
    string config_filename;

    virtual void SetUp()
    {
        config_filename = config_parser.GetDataDir().string() + "/flex_tmp_config_k5i4jn5t.conf";
        int argc = 2;
        const char *argv[] = {"", "-conf=flex_tmp_config_k5i4jn5t.conf"};
        config_parser.ParseCommandLineArguments(argc, argv);
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
    ASSERT_THAT(config_parser.config["conf"], Eq("flex_tmp_config_k5i4jn5t.conf"));
}

TEST_F(AConfigParser, ReturnsAConfig)
{
    WriteConfigFile("a=b\n");
    config_parser.ReadConfigFile();
    FlexConfig config = config_parser.GetConfig();
    ASSERT_THAT(config["a"], Eq("b"));
}