#include "gmock/gmock.h"
#include "TeleportConfig.h"

using namespace ::testing;
using namespace std;


class ATeleportConfig : public Test
{
public:
    TeleportConfig config;
    stringstream stream;
};

TEST_F(ATeleportConfig, InitiallyContainsTheDefaultSettings)
{
    uint64_t size = config.size();
    ASSERT_THAT(size, Eq(default_settings.size()));
}

TEST_F(ATeleportConfig, IncreasesInSizeWhenSomethingIsSet)
{
    config["x"] = "y";
    ASSERT_THAT(config.size(), Gt(default_settings.size()));
}

TEST_F(ATeleportConfig, StoresData)
{
    config["x"] = "y";
    ASSERT_THAT(config["x"], Eq("y"));
}

TEST_F(ATeleportConfig, ReadsDataFromAStream)
{
    stream << "x=y" << endl;
    config.ReadFromStream(stream);
    ASSERT_THAT(config["x"], Eq("y"));
}

TEST_F(ATeleportConfig, ParsesCommandLineArguments)
{
    int argc = 3;
    const char *argv[] = {"", "-foo=1", "-bar=2"};
    config.ParseCommandLineArguments(argc, argv);
    ASSERT_THAT(config["foo"], Eq("1"));
    ASSERT_THAT(config["bar"], Eq("2"));
}

TEST_F(ATeleportConfig, AcceptsFlagsInCommandLineArguments)
{
    int argc = 2;
    const char *argv[] = {"", "-foo"};
    config.ParseCommandLineArguments(argc, argv);
    ASSERT_TRUE(config.settings.count("foo"));
}

TEST_F(ATeleportConfig, NegatesArgumentsBeginningWithNo)
{
    int argc = 2;
    const char *argv[] = {"", "-nofoo"};
    config.ParseCommandLineArguments(argc, argv);
    ASSERT_THAT(config["foo"], Eq("0"));
}

TEST_F(ATeleportConfig, NegatesSettingsReadFromFileBeginningWithNo)
{
    stream << "nofoo=1" << endl;
    config.ReadFromStream(stream);
    ASSERT_THAT(config["foo"], Eq("0"));
}

TEST_F(ATeleportConfig, ProvidesTheSpecifiedDefaultValueOfABooleanSetting)
{
    bool setting_value = config.Value("foo", false);
    ASSERT_THAT(setting_value, Eq(false));
    setting_value = config.Value("foo", true);
    ASSERT_THAT(setting_value, Eq(true));
}

TEST_F(ATeleportConfig, ProvidesTheValueOfABooleanSetting)
{
    stream << "foo=1" << endl;
    config.ReadFromStream(stream);
    bool setting_value = config.Value("foo", false);
    ASSERT_THAT(setting_value, Eq(true));
}

TEST_F(ATeleportConfig, ProvidesTheValueOfAPositiveBooleanSettingSpecifiedNegatively)
{
    stream << "nofoo=0" << endl;
    config.ReadFromStream(stream);
    bool setting_value = config.Value("foo", false);
    ASSERT_THAT(setting_value, Eq(true));
}

TEST_F(ATeleportConfig, ProvidesTheValueOfANegativeBooleanSettingSpecifiedNegatively)
{
    stream << "nofoo=1" << endl;
    config.ReadFromStream(stream);
    bool setting_value = config.Value("foo", true);
    ASSERT_THAT(setting_value, Eq(false));
}

TEST_F(ATeleportConfig, ProvidesTheValueOfAnIntegerSetting)
{
    stream << "foo=5" << endl;
    config.ReadFromStream(stream);
    uint64_t setting_value = config.Value("foo", (uint64_t)0);
    ASSERT_THAT(setting_value, Eq(5));
}

TEST_F(ATeleportConfig, ProvidesTheValueOfAStringSetting)
{
    stream << "foo=5" << endl;
    config.ReadFromStream(stream);
    string setting_value = config.Value("foo", string(""));
    ASSERT_THAT(setting_value, Eq("5"));
}

