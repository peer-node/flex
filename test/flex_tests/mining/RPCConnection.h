#ifndef FLEX_RPCCONNECTION_H
#define FLEX_RPCCONNECTION_H

#include <src/crypto/uint256.h>
#include "json/json_spirit_writer_template.h"


class RPCConnection
{
public:
    uint256 network_id;
    std::string network_ip;
    uint32_t network_port;
    std::vector<std::pair<std::string, json_spirit::Array> > requests;

    RPCConnection():
            network_id(0),
            network_ip(""),
            network_port(0)
    { }

    RPCConnection(uint256 network_id, std::string network_ip, uint32_t network_port):
            network_id(network_id),
            network_ip(network_ip),
            network_port(network_port)
    { }

    virtual uint64_t NumberOfRequests()
    {
        return requests.size();
    }

    virtual json_spirit::Value SendRPCRequest(std::string method, json_spirit::Array params);
};


#endif //FLEX_RPCCONNECTION_H
