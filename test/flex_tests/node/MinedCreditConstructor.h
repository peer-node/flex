#ifndef FLEX_MINEDCREDITCONSTRUCTOR_H
#define FLEX_MINEDCREDITCONSTRUCTOR_H


#include <test/flex_tests/mined_credits/MinedCredit.h>
#include <test/flex_tests/flex_data/MemoryDataStore.h>

class MinedCreditConstructor
{
public:
    MemoryDataStore &keydata, &creditdata, &msgdata;

    MinedCreditConstructor(MemoryDataStore& keydata, MemoryDataStore& creditdata, MemoryDataStore& msgdata) :
        keydata(keydata), creditdata(creditdata), msgdata(msgdata)
    { }

    MinedCredit ConstructMinedCredit();

    Point NewPublicKey();
};


#endif //FLEX_MINEDCREDITCONSTRUCTOR_H
