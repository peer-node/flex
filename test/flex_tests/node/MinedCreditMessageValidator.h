#ifndef FLEX_MINEDCREDITMESSAGEVALIDATOR_H
#define FLEX_MINEDCREDITMESSAGEVALIDATOR_H


#include "CreditSystem.h"

class MinedCreditMessageValidator
{
public:
    CreditSystem *credit_system;

    void SetCreditSystem(CreditSystem *credit_system_)
    {
        credit_system = credit_system_;
    }

    bool HasMissingData(MinedCreditMessage& msg);

    bool CheckBatchRoot(MinedCreditMessage& msg);

    bool CheckBatchSize(MinedCreditMessage& msg);

    bool CheckBatchOffset(MinedCreditMessage &msg);

    bool CheckBatchNumber(MinedCreditMessage &msg);

    bool CheckMessageListHash(MinedCreditMessage &msg);

    bool CheckSpentChainHash(MinedCreditMessage &msg);

    bool CheckTimeStampSucceedsPreviousTimestamp(MinedCreditMessage &msg);

    bool CheckTimeStampIsNotInFuture(MinedCreditMessage &msg);

    bool CheckPreviousTotalWork(MinedCreditMessage &msg);

    bool CheckDiurnalDifficulty(MinedCreditMessage &msg);

    MinedCredit GetPreviousMinedCredit(MinedCreditMessage &msg);

    bool CheckDifficulty(MinedCreditMessage &msg);

    bool CheckPreviousDiurnRoot(MinedCreditMessage &msg);

    bool CheckDiurnalBlockRoot(MinedCreditMessage &msg);

    bool ValidateNetworkState(MinedCreditMessage &msg);

    bool CheckMessageListContainsPreviousMinedCreditHash(MinedCreditMessage &msg);
};


#endif //FLEX_MINEDCREDITMESSAGEVALIDATOR_H
