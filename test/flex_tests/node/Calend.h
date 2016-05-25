#ifndef FLEX_CALEND_H
#define FLEX_CALEND_H


#include "MinedCreditMessage.h"

class Calend : public MinedCreditMessage
{
public:
    Calend () {}

    Calend(MinedCreditMessage msg);

    uint160 Root() const;

    uint160 TotalCreditWork();

    uint160 MinedCreditHash();

    bool operator!=(Calend other);
};


#endif //FLEX_CALEND_H
