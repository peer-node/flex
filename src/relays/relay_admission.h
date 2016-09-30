// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#ifndef TELEPORT_RELAY_ADMISSION
#define TELEPORT_RELAY_ADMISSION

#include "relays/relay_messages.h"


void DoScheduledJoinCheck(uint160 join_hash);

void DoScheduledJoinComplaintCheck(uint160 complaint_hash);

void DoScheduledFutureSuccessorComplaintCheck(uint160 complaint_hash);

void DoScheduledSuccessionCheck(uint160 complaint_hash);

void DoScheduledSuccessionComplaintCheck(uint160 complaint_hash);

void AddRelayDutyForBatch(uint160 credit_hash);

#endif