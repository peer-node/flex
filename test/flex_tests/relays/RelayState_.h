#ifndef FLEX_NETWORKRELAYSTATE_H
#define FLEX_NETWORKRELAYSTATE_H

#include "Relay.h"

class RelayState_
{
public:
    std::vector<Relay> relays;

    void AddRelay(Relay relay);
};


#endif //FLEX_NETWORKRELAYSTATE_H
