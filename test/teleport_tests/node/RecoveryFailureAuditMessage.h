#ifndef TELEPORT_RECOVERYFAILUREAUDITMESSAGE_H
#define TELEPORT_RECOVERYFAILUREAUDITMESSAGE_H


#include <src/crypto/uint256.h>
#include <src/crypto/point.h>
#include <src/crypto/signature.h>
#include "Data.h"
#include "SecretRecoveryFailureMessage.h"
#include "SecretRecoveryMessage.h"


class RecoveryFailureAuditMessage
{
public:
    uint160 failure_message_hash;
    uint64_t quarter_holder_number;
    CBigNum private_receiving_key_quarter;
    Signature signature;

    static std::string Type() { return "recovery_failure_audit"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(failure_message_hash);
        READWRITE(quarter_holder_number);
        READWRITE(private_receiving_key_quarter);
        READWRITE(signature);
    );

    JSON(failure_message_hash, quarter_holder_number, private_receiving_key_quarter, signature);

    DEPENDENCIES(failure_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    void Populate(SecretRecoveryFailureMessage message, SecretRecoveryMessage recoveryMessage, Data data);

    CBigNum GetReceivingKeyQuarter(SecretRecoveryFailureMessage failure_message,
                                   SecretRecoveryMessage recovery_message, Data data);

    int64_t GetKeySixteenthPosition(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message,
                                    Data data);

    Relay *GetKeySharer(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message, Data data);

    bool VerifyPrivateReceivingKeyQuarterMatchesPublicReceivingKeyQuarter(Data data);

    Point
    GetPublicKeySixteenth(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message, Data data);

    SecretRecoveryMessage GetSecretRecoveryMessage(Data data);

    uint8_t
    GetKeyQuarterPosition(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message, Data data);

    bool VerifyEncryptedSharedSecretQuarterInSecretRecoveryMessageWasCorrect(Data data);

    Point GetSharedSecretQuarter(Data data);

    CBigNum GetEncryptedSharedSecretQuarterFromSecretRecoveryMessage(Data data);

    SecretRecoveryFailureMessage GetSecretRecoveryFailureMessage(Data data);

    Point GetPublicKeySixteenth(Data data);

    int64_t GetKeySixteenthPosition(Data data);

    Relay *GetDeadRelay(Data data);

    Relay *GetKeySharer(Data data);

    bool IsValid(Data data);
};


#endif //TELEPORT_RECOVERYFAILUREAUDITMESSAGE_H
