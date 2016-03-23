#include "FlexNode.h"

void FlexNode::LoadConfig(int argc, const char **argv)
{
    config_parser.ParseCommandLineArguments(argc, argv);
    config_parser.ReadConfigFile();
    config = config_parser.GetConfig();
}

