#include "KeyDistributionRefutation.h"
#include "KeyDistributionComplaint.h"
#include "Relay.h"

KeyDistributionComplaint KeyDistributionRefutation::GetComplaint(Data data)
{
    return data.msgdata[complaint_hash]["key_distribution_complaint"];
}

Point KeyDistributionRefutation::VerificationKey(Data data)
{
    Relay *secret_sender = GetComplaint(data).GetSecretSender(data);
    return secret_sender->public_key;
}

Relay *KeyDistributionRefutation::GetComplainer(Data data)
{
    return GetComplaint(data).GetComplainer(data);
}

Relay *KeyDistributionRefutation::GetSecretSender(Data data)
{
    return GetComplaint(data).GetSecretSender(data);
}
