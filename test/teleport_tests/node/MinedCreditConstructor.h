#ifndef TELEPORT_MINEDCREDITCONSTRUCTOR_H
#define TELEPORT_MINEDCREDITCONSTRUCTOR_H


#include <test/teleport_tests/mined_credits/MinedCredit.h>
#include <test/teleport_tests/teleport_data/MemoryDataStore.h>

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


#endif //TELEPORT_MINEDCREDITCONSTRUCTOR_H
