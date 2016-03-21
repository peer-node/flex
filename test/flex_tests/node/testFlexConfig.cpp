#include "gmock/gmock.h"
#include "FlexConfig.h"

using namespace ::testing;
using namespace std;


class AFlexConfig : public Test
{
public:
    FlexConfig config;
    stringstream stream;
};

TEST_F(AFlexConfig, IsEmptyInitially)
{
    uint64_t size = config.size();
    ASSERT_THAT(size, Eq(0));
}

TEST_F(AFlexConfig, IsNotEmptyWhenSomethingIsSet)
{
    config["x"] = "y";
    ASSERT_THAT(config.size(), Eq(1));
}

TEST_F(AFlexConfig, StoresData)
{
    config["x"] = "y";
    ASSERT_THAT(config["x"], Eq("y"));
}

TEST_F(AFlexConfig, ReadsDataFromAStream)
{
    stream << "x=y" << endl;
    config.ReadFromStream(stream);
    ASSERT_THAT(config["x"], Eq("y"));
}

TEST_F(AFlexConfig, ParsesCommandLineArguments)
{
    int argc = 3;
    const char *argv[] = {"", "-foo=1", "-bar=2"};
    config.ParseCommandLineArguments(argc, argv);
    ASSERT_THAT(config["foo"], Eq("1"));
    ASSERT_THAT(config["bar"], Eq("2"));
}

TEST_F(AFlexConfig, AcceptsFlagsInCommandLineArguments)
{
    int argc = 2;
    const char *argv[] = {"", "-foo"};
    config.ParseCommandLineArguments(argc, argv);
    ASSERT_TRUE(config.settings.count("foo"));
}

TEST_F(AFlexConfig, NegatesArgumentsBeginningWithNo)
{
    int argc = 2;
    const char *argv[] = {"", "-nofoo"};
    config.ParseCommandLineArguments(argc, argv);
    ASSERT_THAT(config["foo"], Eq("0"));
}

TEST_F(AFlexConfig, NegatesSettingsReadFromFileBeginningWithNo)
{
    stream << "nofoo=1" << endl;
    config.ReadFromStream(stream);
    ASSERT_THAT(config["foo"], Eq("0"));
}

TEST_F(AFlexConfig, ProvidesTheSpecifiedDefaultValueOfABooleanSetting)
{
    bool setting_value = config.Value("foo", false);
    ASSERT_THAT(setting_value, Eq(false));
    setting_value = config.Value("foo", true);
    ASSERT_THAT(setting_value, Eq(true));
}

TEST_F(AFlexConfig, ProvidesTheValueOfABooleanSetting)
{
    stream << "foo=1" << endl;
    config.ReadFromStream(stream);
    bool setting_value = config.Value("foo", false);
    ASSERT_THAT(setting_value, Eq(true));
}

TEST_F(AFlexConfig, ProvidesTheValueOfAPositiveBooleanSettingSpecifiedNegatively)
{
    stream << "nofoo=0" << endl;
    config.ReadFromStream(stream);
    bool setting_value = config.Value("foo", false);
    ASSERT_THAT(setting_value, Eq(true));
}

TEST_F(AFlexConfig, ProvidesTheValueOfANegativeBooleanSettingSpecifiedNegatively)
{
    stream << "nofoo=1" << endl;
    config.ReadFromStream(stream);
    bool setting_value = config.Value("foo", true);
    ASSERT_THAT(setting_value, Eq(false));
}

TEST_F(AFlexConfig, ProvidesTheValueOfAnIntegerSetting)
{
    stream << "foo=5" << endl;
    config.ReadFromStream(stream);
    uint64_t setting_value = config.Value("foo", (uint64_t)0);
    ASSERT_THAT(setting_value, Eq(5));
}

TEST_F(AFlexConfig, ProvidesTheValueOfAStringSetting)
{
    stream << "foo=5" << endl;
    config.ReadFromStream(stream);
    string setting_value = config.Value("foo", string(""));
    ASSERT_THAT(setting_value, Eq("5"));
}

