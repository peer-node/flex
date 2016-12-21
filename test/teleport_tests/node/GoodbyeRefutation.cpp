#include "GoodbyeRefutation.h"
#include "GoodbyeComplaint.h"


Point GoodbyeRefutation::VerificationKey(Data data)
{
    Relay *secret_sender = GetComplaint(data).GetSecretSender(data);
    return secret_sender->public_key;
}

GoodbyeComplaint GoodbyeRefutation::GetComplaint(Data data)
{
    return data.msgdata[complaint_hash]["goodbye_complaint"];
}