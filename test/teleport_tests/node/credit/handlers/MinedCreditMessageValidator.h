#ifndef TELEPORT_MINEDCREDITMESSAGEVALIDATOR_H
#define TELEPORT_MINEDCREDITMESSAGEVALIDATOR_H


#include "test/teleport_tests/node/credit/structures/CreditSystem.h"

class Data;

class MinedCreditMessageValidator
{
public:
    CreditSystem *credit_system;
    Data *data;

    void SetCreditSystem(CreditSystem *credit_system_)
    {
        credit_system = credit_system_;
        SetData(&credit_system_->data);
    }

    void SetData(Data *data_)
    {
        data = data_;
    }

    bool HasMissingData(MinedCreditMessage& msg);

    bool CheckBatchRoot(MinedCreditMessage& msg);

    bool CheckBatchSize(MinedCreditMessage& msg);

    bool CheckBatchOffset(MinedCreditMessage &msg);

    bool CheckBatchNumber(MinedCreditMessage &msg);

    bool CheckMessageListHash(MinedCreditMessage &msg);

    bool CheckSpentChainHash(MinedCreditMessage &msg);

    bool CheckRelayStateHash(MinedCreditMessage &msg);

    bool CheckTimeStampSucceedsPreviousTimestamp(MinedCreditMessage &msg);

    bool CheckTimeStampIsNotInFuture(MinedCreditMessage &msg);

    bool CheckPreviousTotalWork(MinedCreditMessage &msg);

    bool CheckDiurnalDifficulty(MinedCreditMessage &msg);

    bool CheckDifficulty(MinedCreditMessage &msg);

    bool CheckPreviousDiurnRoot(MinedCreditMessage &msg);

    bool CheckDiurnalBlockRoot(MinedCreditMessage &msg);

    bool ValidateNetworkState(MinedCreditMessage &msg);

    bool CheckMessageListContainsPreviousMinedCreditHash(MinedCreditMessage &msg);

    bool DataRequiredToCalculateDifficultyIsPresent(MinedCreditMessage &msg);

    bool DataRequiredToCalculateDiurnalBlockRootIsPresent(MinedCreditMessage &msg);

    MinedCreditMessage GetPreviousMinedCreditMessage(MinedCreditMessage &msg);

    bool CheckPreviousCalendHash(MinedCreditMessage msg);

    bool DataRequiredToCalculateDiurnalDifficultyIsPresent(MinedCreditMessage &msg);
};


#endif //TELEPORT_MINEDCREDITMESSAGEVALIDATOR_H
