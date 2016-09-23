
#ifndef FLEX_LISTEXPANSIONREQUESTMESSAGE_H
#define FLEX_LISTEXPANSIONREQUESTMESSAGE_H

#include "MinedCreditMessage.h"

class ListExpansionRequestMessage
{
public:
    uint160 mined_credit_message_hash;

    ListExpansionRequestMessage() { }

    ListExpansionRequestMessage(MinedCreditMessage msg)
    {
        mined_credit_message_hash = msg.GetHash160();
    }

    static std::string Type() { return "list_expansion_request"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(mined_credit_message_hash);
    )

    DEPENDENCIES();

    HASH160();
};


#endif //FLEX_LISTEXPANSIONREQUESTMESSAGE_H
