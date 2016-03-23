#include <fstream>
#include "gmock/gmock.h"
#include "FlexNode.h"

using namespace ::testing;
using namespace std;


class AFlexNode : public Test
{
public:
    FlexNode flexnode;
    string config_filename;

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
    int argc = 2;
    const char *argv[] = {"", "-conf=flex_tmp_config_k5i4jn5u.conf"};
    WriteConfigFile("a=b");
    flexnode.LoadConfig(argc, argv);
    ASSERT_THAT(flexnode.config["a"], Eq("b"));
}