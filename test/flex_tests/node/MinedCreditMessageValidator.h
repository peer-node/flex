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
};


#endif //FLEX_MINEDCREDITMESSAGEVALIDATOR_H
