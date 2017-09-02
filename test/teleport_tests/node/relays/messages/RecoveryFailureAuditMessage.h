#ifndef TELEPORT_RECOVERYFAILUREAUDITMESSAGE_H
#define TELEPORT_RECOVERYFAILUREAUDITMESSAGE_H


#include <src/crypto/uint256.h>
#include <src/crypto/point.h>
#include <src/crypto/signature.h>
#include "test/teleport_tests/node/Data.h"
#include "SecretRecoveryFailureMessage.h"
#include "SecretRecoveryMessage.h"


class RecoveryFailureAuditMessage
{
public:
    uint64_t successor_number{0};
    uint160 failure_message_hash{0};
    uint64_t quarter_holder_number{0};
    uint8_t position_of_quarter_holder{0};
    uint256 private_receiving_key_quarter{0};
    Signature signature;

    static std::string Type() { return "recovery_failure_audit"; }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(successor_number);
        READWRITE(failure_message_hash);
        READWRITE(quarter_holder_number);
        READWRITE(position_of_quarter_holder);
        READWRITE(private_receiving_key_quarter);
        READWRITE(signature);
    );

    JSON(successor_number, failure_message_hash, quarter_holder_number, position_of_quarter_holder,
         private_receiving_key_quarter, signature);

    DEPENDENCIES(failure_message_hash);

    IMPLEMENT_HASH_SIGN_VERIFY();

    Point VerificationKey(Data data);

    void Populate(SecretRecoveryFailureMessage message, SecretRecoveryMessage recoveryMessage, Data data);

    CBigNum GetReceivingKeyQuarter(SecretRecoveryFailureMessage failure_message,
                                   SecretRecoveryMessage recovery_message, Data data);

    int64_t GetKeyTwentyFourthPosition(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message,
                                    Data data);

    Relay *GetKeySharer(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message, Data data);

    bool VerifyPrivateReceivingKeyQuarterMatchesPublicReceivingKeyQuarter(Data data);

    Point
    GetPublicKeyTwentyFourth(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message, Data data);

    SecretRecoveryMessage GetSecretRecoveryMessage(Data data);

    uint8_t
    GetKeyQuarterPosition(SecretRecoveryFailureMessage failure_message, SecretRecoveryMessage recovery_message, Data data);

    bool VerifyEncryptedSharedSecretQuarterInSecretRecoveryMessageWasCorrect(Data data);

    Point GetSharedSecretQuarter(Data data);

    uint256 GetEncryptedSharedSecretQuarterFromSecretRecoveryMessage(Data data);

    SecretRecoveryFailureMessage GetSecretRecoveryFailureMessage(Data data);

    Point GetPublicKeyTwentyFourth(Data data);

    int64_t GetKeyTwentyFourthPosition(Data data);

    Relay *GetDeadRelay(Data data);

    Relay *GetKeySharer(Data data);

    bool IsValid(Data data);

    Relay *GetKeyQuarterHolder(Data data);

    bool ContainsCorrectData(Data data);
};


#endif //TELEPORT_RECOVERYFAILUREAUDITMESSAGE_H
