#ifndef TELEPORT_DEPOSITADDRESSTRANSFERHANDLER_H
#define TELEPORT_DEPOSITADDRESSTRANSFERHANDLER_H


#include <src/teleportnode/schedule.h>
#include "test/teleport_tests/node/deposits/messages/DepositTransferMessage.h"
#include "test/teleport_tests/node/deposits/messages/TransferAcknowledgement.h"

class DepositMessageHandler;
class TeleportNetworkNode;

class DepositMessageHandler;

class DepositAddressTransferHandler
{
public:
    DepositMessageHandler *deposit_message_handler{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    Scheduler scheduler;
    Data data;

    explicit DepositAddressTransferHandler(DepositMessageHandler *deposit_message_handler);

    void HandleDepositTransferMessage(DepositTransferMessage message);

    void HandleTransferAcknowledgement(TransferAcknowledgement acknowledgement);

    void SetNetworkNode(TeleportNetworkNode *node);
};


#endif //TELEPORT_DEPOSITADDRESSTRANSFERHANDLER_H
