#ifndef TELEPORT_TELEPORTRPCSERVER_H
#define TELEPORT_TELEPORTRPCSERVER_H


#include <jsonrpccpp/server/connectors/httpserver.h>
#include <jsonrpccpp/server/abstractserver.h>
#include <src/crypto/uint256.h>
#include <test/teleport_tests/node/Data.h>
#include "HttpAuthServer.h"


class TeleportLocalServer;


class TeleportRPCServer : public jsonrpc::AbstractServer<TeleportRPCServer>
{
public:
    uint256 network_id;
    std::map<std::string,std::string> headers;
    std::string response{"a response"};
    TeleportLocalServer *teleport_local_server{NULL};
    std::vector<std::string> methods;
    Data *data{NULL};

    TeleportRPCServer(jsonrpc::HttpAuthServer &server) :
            jsonrpc::AbstractServer<TeleportRPCServer>(server)
    {
        BindMethod("help", &TeleportRPCServer::Help);
        BindMethod("getinfo", &TeleportRPCServer::GetInfo);
        BindMethod("setnetworkid", &TeleportRPCServer::SetNetworkID);
        BindMethod("set_megabytes_used", &TeleportRPCServer::SetMegabytesUsed);
        BindMethod("new_proof", &TeleportRPCServer::NewProof);
        BindMethod("balance", &TeleportRPCServer::Balance);
        BindMethod("start_mining", &TeleportRPCServer::StartMining);
        BindMethod("start_mining_asynchronously", &TeleportRPCServer::StartMiningAsynchronously);
        BindMethod("sendtopublickey", &TeleportRPCServer::SendToPublicKey);
        BindMethod("getnewaddress", &TeleportRPCServer::GetNewAddress);
        BindMethod("sendtoaddress", &TeleportRPCServer::SendToAddress);
        BindMethod("addnode", &TeleportRPCServer::AddNode);
        BindMethod("requesttips", &TeleportRPCServer::RequestTips);
        BindMethod("getcalendar", &TeleportRPCServer::GetCalendar);
        BindMethod("getminedcredit", &TeleportRPCServer::GetMinedCredit);
        BindMethod("getminedcreditmessage", &TeleportRPCServer::GetMinedCreditMessage);
        BindMethod("getbatch", &TeleportRPCServer::GetBatch);
        BindMethod("listunspent", &TeleportRPCServer::ListUnspent);
        BindMethod("gettransaction", &TeleportRPCServer::GetTransaction);

        BindMethod("requestdepositaddress", &TeleportRPCServer::RequestDepositAddress);
        BindMethod("listdepositaddresses", &TeleportRPCServer::ListDepositAddresses);
    }

    void SetTeleportLocalServer(TeleportLocalServer *teleport_local_server_);

    void Help(const Json::Value& request, Json::Value& response);

    void GetInfo(const Json::Value& request, Json::Value& response);

    void SetNetworkID(const Json::Value& request, Json::Value& response);

    void SetMegabytesUsed(const Json::Value& request, Json::Value& response);

    void NewProof(const Json::Value& request, Json::Value& response);

    void Balance(const Json::Value& request, Json::Value& response);

    void StartMining(const Json::Value& request, Json::Value& response);

    void StartMiningAsynchronously(const Json::Value& request, Json::Value& response);

    void BindMethod(const char* method_name,
                    void (TeleportRPCServer::*method)(const Json::Value &,Json::Value &));

    void SendToPublicKey(const Json::Value &request, Json::Value &response);

    void GetNewAddress(const Json::Value &request, Json::Value &response);

    void SendToAddress(const Json::Value &request, Json::Value &response);

    void AddNode(const Json::Value &request, Json::Value &response);

    void RequestTips(const Json::Value &request, Json::Value &response);

    void GetCalendar(const Json::Value &request, Json::Value &response);

    void GetMinedCredit(const Json::Value &request, Json::Value &response);

    void GetMinedCreditMessage(const Json::Value &request, Json::Value &response);

    void GetBatch(const Json::Value &request, Json::Value &response);

    void ListUnspent(const Json::Value &request, Json::Value &response);

    void GetTransaction(const Json::Value &request, Json::Value &response);

    void RequestDepositAddress(const Json::Value &request, Json::Value &response);

    void ListDepositAddresses(const Json::Value &request, Json::Value &response);
};


#endif //TELEPORT_TELEPORTRPCSERVER_H
