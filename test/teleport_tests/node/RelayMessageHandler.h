#ifndef TELEPORT_RELAYMESSAGEHANDLER_H
#define TELEPORT_RELAYMESSAGEHANDLER_H


#include <test/teleport_tests/teleport_data/MemoryDataStore.h>
#include "RelayState.h"
#include "CreditSystem.h"
#include "MessageHandlerWithOrphanage.h"
#include "RelayJoinMessage.h"


class RelayMessageHandler : public MessageHandlerWithOrphanage
{
public:
    MemoryDataStore &msgdata, &creditdata, &keydata;
    CreditSystem *credit_system;
    Calendar *calendar;
    RelayState relay_state;

    RelayMessageHandler(MemoryDataStore &msgdata_, MemoryDataStore &creditdata_, MemoryDataStore &keydata_):
        MessageHandlerWithOrphanage(msgdata_), creditdata(creditdata_), keydata(keydata_), msgdata(msgdata_)
    {
        channel = "relay";
    }

    void SetCreditSystem(CreditSystem *credit_system);

    void SetCalendar(Calendar *calendar);

    void HandleMessage(CDataStream ss, CNode* peer)
    {
        HANDLESTREAM(RelayJoinMessage);
    }

    void HandleMessage(uint160 message_hash)
    {
        HANDLEHASH(RelayJoinMessage);
    }

    HANDLECLASS(RelayJoinMessage);

    void HandleRelayJoinMessage(RelayJoinMessage relay_join_message);

    bool MinedCreditMessageHashIsInMainChain(RelayJoinMessage relay_join_message);
};


#endif //TELEPORT_RELAYMESSAGEHANDLER_H
