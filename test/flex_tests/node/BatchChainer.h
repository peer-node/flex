#ifndef FLEX_BATCHCHAINER_H
#define FLEX_BATCHCHAINER_H


#include "CreditSystem.h"
#include "MinedCreditMessageValidator.h"

class BatchChainer
{
public:
    CreditSystem *credit_system;
    MinedCreditMessageValidator *mined_credit_message_validator;
    uint160 latest_mined_credit_hash{0};

    void SetCreditSystem(CreditSystem *credit_system_);

    uint160 LatestMinedCreditHash();

    void AddBatchToTip(MinedCreditMessage &msg);

    void RemoveBatchFromTip();

    void SetMinedCreditMessageValidator(MinedCreditMessageValidator *mined_credit_message_validator_);

    bool ValidateMinedCreditMessage(MinedCreditMessage &msg);
};


#endif //FLEX_BATCHCHAINER_H
