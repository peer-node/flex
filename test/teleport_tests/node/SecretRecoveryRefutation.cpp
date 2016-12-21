#include "SecretRecoveryRefutation.h"
#include "Data.h"
#include "SecretRecoveryComplaint.h"


Point SecretRecoveryRefutation::VerificationKey(Data data)
{
    Relay *secret_sender = GetComplaint(data).GetSecretSender(data);
    return secret_sender->public_key;
}

SecretRecoveryComplaint SecretRecoveryRefutation::GetComplaint(Data data)
{
    return data.msgdata[complaint_hash]["secret_recovery_complaint"];
}