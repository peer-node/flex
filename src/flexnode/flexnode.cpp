
#include "flexnode/flexnode.h"
#include "log.h"

#define LOG_CATEGORY "flexnode.cpp"

using namespace std;

/**************
 *  FlexNode
 */

    void FlexNode::Test()
    {
        log_ << "This is the flexnode test." << "\n";
        string address_string("19b2JzjCvzfnFjour7EUvALPtTqxGVpFGt");
        vch_t data;
        DecodeBase58Check(address_string, data);
        log_ << "data is " << data << "\n";
        
        CBitcoinAddress address(address_string);
        CKeyID keyid;
        address.GetKeyID(keyid);
        vch_t keydata_(BEGIN(keyid), END(keyid));
        uint160 address_hash(vch_t(BEGIN(keyid), END(keyid)));
        log_<<"address_hash is "<<address_hash<<"\n";
        log_ << "keydata is " << keydata_ << "\n";
        uint160 hash(keydata_);
        log_ << "hash is " << hash << "\n";
        CKeyID keyID(hash);
        log_ << "btc address is " << CBitcoinAddress(keyID).ToString() << "\n";
    }

/*
 *  FlexNode
 **************/
