#ifndef TELEPORT_DEPOSITADDRESSDISCLOSUREHANDLER_H
#define TELEPORT_DEPOSITADDRESSDISCLOSUREHANDLER_H


#include "test/teleport_tests/node/deposits/messages/DepositAddressPartDisclosure.h"
#include "test/teleport_tests/node/deposits/messages/DepositDisclosureComplaint.h"

class DepositAddressDisclosureHandler
{
public:

    void HandleDepositAddressPartDisclosure(DepositAddressPartDisclosure disclosure);

    void HandleDepositDisclosureComplaint(DepositDisclosureComplaint complaint);
};


#endif //TELEPORT_DEPOSITADDRESSDISCLOSUREHANDLER_H
