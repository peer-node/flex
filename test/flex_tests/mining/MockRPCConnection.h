#ifndef FLEX_MOCKRPCCONNECTION_H
#define FLEX_MOCKRPCCONNECTION_H

#include <src/crypto/uint256.h>
#include "json/json_spirit_writer_template.h"
#include "RPCConnection.h"

class MockRPCConnection : public RPCConnection
{
public:
    virtual json_spirit::Value SendRPCRequest(std::string method, json_spirit::Array params);
};


#endif //FLEX_MOCKRPCCONNECTION_H
