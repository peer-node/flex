#ifndef FLEX_CREDITMESSAGEHANDLER_H
#define FLEX_CREDITMESSAGEHANDLER_H

class CDataStream;
class CNode;
class MainFlexNode;

class CreditMessageHandler
{
public:
    uint64_t messages_received{0};
    MainFlexNode *flexnode;

    void HandleMessage(const char *command, CDataStream stream, CNode *peer);

    void SetFlexNode(MainFlexNode &node);
};


#endif //FLEX_CREDITMESSAGEHANDLER_H
