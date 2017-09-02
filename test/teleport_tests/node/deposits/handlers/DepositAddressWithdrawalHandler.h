#ifndef TELEPORT_DEPOSITADDRESSWITHDRAWALHANDLER_H
#define TELEPORT_DEPOSITADDRESSWITHDRAWALHANDLER_H


#include "test/teleport_tests/node/deposits/messages/WithdrawalRequestMessage.h"
#include "test/teleport_tests/node/deposits/messages/WithdrawalMessage.h"
#include "test/teleport_tests/node/deposits/messages/WithdrawalComplaint.h"
#include <test/teleport_tests/node/TeleportNetworkNode.h>

class DepositMessageHandler;

class DepositAddressWithdrawalHandler
{
public:
    DepositMessageHandler *deposit_message_handler{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    Scheduler scheduler;
    Data data;

    void HandleWithdrawalRequestMessage(WithdrawalRequestMessage message);
    bool ValidateWithdrawalRequestMessage(WithdrawalRequestMessage &message);
    void AcceptWithdrawalRequestMessage(WithdrawalRequestMessage &message);

    void HandleWithdrawalMessage(WithdrawalMessage message);
    bool ValidateWithdrawalMessage(WithdrawalMessage &message);
    void AcceptWithdrawalMessage(WithdrawalMessage &message);

    void HandleWithdrawalComplaint(WithdrawalComplaint complaint);
    bool ValidateWithdrawalComplaint(WithdrawalComplaint &complaint);
    void AcceptWithdrawalComplaint(WithdrawalComplaint &complaint);
};


#endif //TELEPORT_DEPOSITADDRESSWITHDRAWALHANDLER_H
