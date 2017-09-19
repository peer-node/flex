#ifndef TELEPORT_DEPOSITADDRESSDISCLOSUREHANDLER_H
#define TELEPORT_DEPOSITADDRESSDISCLOSUREHANDLER_H


#include <src/teleportnode/schedule.h>
#include "test/teleport_tests/node/deposits/messages/DepositAddressPartDisclosure.h"
#include "test/teleport_tests/node/deposits/messages/DepositDisclosureComplaint.h"

class DepositMessageHandler;
class TeleportNetworkNode;


class DepositAddressDisclosureHandler
{
public:
    DepositMessageHandler *deposit_message_handler{NULL};
    TeleportNetworkNode *teleport_network_node{NULL};
    Scheduler scheduler;
    Data data;

    explicit DepositAddressDisclosureHandler(DepositMessageHandler *deposit_message_handler);

    void AddScheduledTasks();

    void HandleDepositAddressPartDisclosure(DepositAddressPartDisclosure disclosure);

    void HandleDepositDisclosureComplaint(DepositDisclosureComplaint complaint);

    bool ValidateDepositAddressPartDisclosure(DepositAddressPartDisclosure &disclosure);

    void RecordDepositAddressPartDisclosure(DepositAddressPartDisclosure &disclosure);

    bool AllDisclosuresHaveBeenReceived(uint160 encoded_request_identifier);

    void HandleReceiptOfAllDisclosures(uint160 encoded_request_identifier);

    void RecordCompleteDisclosureOfMyRequest(uint160 encoded_request_identifier);

    void ActivateMyDepositAddress(uint160 encoded_request_identifier);

    void SetNetworkNode(TeleportNetworkNode *node);
};


#endif //TELEPORT_DEPOSITADDRESSDISCLOSUREHANDLER_H
