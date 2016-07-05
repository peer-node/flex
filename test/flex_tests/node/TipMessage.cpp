#include "TipMessage.h"
#include "TipRequestMessage.h"

TipMessage::TipMessage(TipRequestMessage tip_request, Calendar *calendar)
{
    mined_credit = calendar->LastMinedCredit();
    tip_request_hash = tip_request.GetHash160();
}

