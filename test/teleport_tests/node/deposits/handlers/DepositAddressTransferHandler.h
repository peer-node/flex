#ifndef TELEPORT_DEPOSITADDRESSTRANSFERHANDLER_H
#define TELEPORT_DEPOSITADDRESSTRANSFERHANDLER_H


#include "test/teleport_tests/node/deposits/messages/DepositTransferMessage.h"
#include "test/teleport_tests/node/deposits/messages/TransferAcknowledgement.h"

class DepositAddressTransferHandler
{
public:

    void HandleDepositTransferMessage(DepositTransferMessage message);

    void HandleTransferAcknowledgement(TransferAcknowledgement acknowledgement);
};


#endif //TELEPORT_DEPOSITADDRESSTRANSFERHANDLER_H
