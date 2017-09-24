#ifndef TELEPORT_DEPOSITADDRESSWITHDRAWALHANDLER_H
#define TELEPORT_DEPOSITADDRESSWITHDRAWALHANDLER_H


#include <src/teleportnode/schedule.h>
#include "test/teleport_tests/node/deposits/messages/WithdrawalRequestMessage.h"
#include "test/teleport_tests/node/deposits/messages/WithdrawalMessage.h"
#include "test/teleport_tests/node/deposits/messages/WithdrawalComplaint.h"


class DepositMessageHandler;
class TeleportNetworkNode;

class DepositAddressWithdrawalHandler
{
public:
    DepositMessageHandler *deposit_message_handler{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    Scheduler scheduler;
    Data data;

    explicit DepositAddressWithdrawalHandler(DepositMessageHandler *deposit_message_handler);


    void HandleWithdrawalRequestMessage(WithdrawalRequestMessage message);
    bool ValidateWithdrawalRequestMessage(WithdrawalRequestMessage &message);
    void AcceptWithdrawalRequestMessage(WithdrawalRequestMessage &message);

    void HandleWithdrawalMessage(WithdrawalMessage message);
    bool ValidateWithdrawalMessage(WithdrawalMessage &message);
    void AcceptWithdrawalMessage(WithdrawalMessage &message);

    void HandleWithdrawalComplaint(WithdrawalComplaint complaint);
    bool ValidateWithdrawalComplaint(WithdrawalComplaint &complaint);
    void AcceptWithdrawalComplaint(WithdrawalComplaint &complaint);

    void SetNetworkNode(TeleportNetworkNode *node);

    void SendWithdrawalRequestMessage(Point deposit_address_point);

    void SendWithdrawalMessage(uint160 request_hash, uint32_t position);

    void HandleCompletedWithdrawal(Point deposit_address_pubkey);

    CBigNum ReconstructDepositAddressPrivateKey(Point deposit_address_pubkey);
};


#endif //TELEPORT_DEPOSITADDRESSWITHDRAWALHANDLER_H
