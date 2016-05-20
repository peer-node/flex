#include "MinedCreditMessage.h"

uint160 MinedCreditMessage::GetHash160()
{
    CDataStream ss(SER_NETWORK, CLIENT_VERSION);
    ss << *this;
    return Hash160(ss.begin(), ss.end());
}

