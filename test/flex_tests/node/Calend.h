#ifndef FLEX_CALEND_H
#define FLEX_CALEND_H


#include "MinedCreditMessage.h"
#include "test/flex_tests/node/data_handler/DiurnMessageData.h"

class CreditSystem;

class Calend : public MinedCreditMessage
{
public:
    Calend () {}

    Calend(MinedCreditMessage msg);

    uint160 DiurnRoot() const;

    uint160 TotalCreditWork();

    DiurnMessageData GenerateDiurnMessageData(CreditSystem *credit_system);
};


#endif //FLEX_CALEND_H
