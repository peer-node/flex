#ifndef FLEX_FLEXNODE_H
#define FLEX_FLEXNODE_H


#include "ConfigParser.h"

class FlexNode
{
public:
    void LoadConfig(int argc, const char *argv[]);

    ConfigParser config_parser;
    FlexConfig config;
};


#endif //FLEX_FLEXNODE_H
