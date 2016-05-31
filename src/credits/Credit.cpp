#include "Credit.h"
#include "crypto/point.h"
#include "database/data.h"
#include "creditsign.h"

using namespace std;

string_t ByteString(vch_t bytes)
{
    string_t result;
    char buffer[BUFSIZ];
    for (uint32_t i = 0; i < bytes.size(); i++)
    {
        sprintf(buffer, "%02x", bytes[i]);
        buffer[2] = 0;
        result = result + buffer;
    }
    return result;
}

/************
 *  Credit
 */

    Credit::Credit(vch_t keydata, uint64_t amount):
        amount(amount),
        keydata(keydata)
    { }

    Credit::Credit(vch_t data)
    {
        memcpy(&amount, &data[0], 8);
        keydata.resize(data.size() - 8);
        memcpy(&keydata[0], &data[8], data.size() - 8);
    }

    string_t Credit::ToString() const
    {
        stringstream ss;

        uint160 keyhash = KeyHash();
        Point pubkey;

        if (keydata.size() == 20)
        {
            //pubkey = ::keydata[keyhash]["pubkey"];
        }
        else
        {
            pubkey.setvch(keydata);
        }

        ss << "\n============== Credit =============" << "\n"
           << "== Amount: " << amount << "\n"
           << "== Key Data: " << ByteString(keydata) << "\n"
           << "== " << "\n"
           << "== Key Hash: " << keyhash.ToString() << "\n"
           << "== Pubkey: " << pubkey.ToString() << "\n"
           << "============ End Credit ===========" << "\n";
        return ss.str();
    }

    uint160 Credit::KeyHash() const
    {
        uint160 keyhash;
        Point pubkey;

        if (keydata.size() == 20)
        {
            keyhash = uint160(keydata);
        }
        else
        {
            pubkey.setvch(keydata);
            keyhash = ::KeyHash(pubkey);
        }
        return keyhash;
    }

    vch_t Credit::getvch() const
    {
        vch_t output;
        output.resize(keydata.size() + 8);
        memcpy(&output[0], &amount, 8);
        memcpy(&output[8], &keydata[0], keydata.size());
        return output;
    }

    bool Credit::operator==(const Credit& other_credit) const
    {
        return amount == other_credit.amount
                && keydata == other_credit.keydata;
    }

    bool Credit::operator<(const Credit& other_credit) const
    {
        return amount < other_credit.amount
                || (amount == other_credit.amount &&
                    keydata < other_credit.keydata);
    }

/*
 *  Credit
 ************/