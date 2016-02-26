#include "MockRPCConnection.h"


using namespace json_spirit;

Value MockRPCConnection::SendRPCRequest(std::string method, Array params)
{
    Value result;
    requests.push_back(std::make_pair(method, params));
    return result;
}
