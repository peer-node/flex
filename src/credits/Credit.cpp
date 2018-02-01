#include "Credit.h"
#include "crypto/point.h"
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

    Credit::Credit(Point public_key, uint64_t amount):
        amount(amount),
        public_key(public_key)
    { }

    Credit::Credit(vch_t data)
    {
        memcpy(&amount, &data[0], 8);
        vch_t key_bytes(data.begin() + 8, data.end());
        public_key.setvch(key_bytes);
    }

    string_t Credit::ToString()
    {
        stringstream ss;

        ss << "\n============== Credit =============" << "\n"
           << "== Amount: " << amount << "\n"
           << "== Address: " << TeleportAddress() << "\n"
           << "============ End Credit ===========" << "\n";
        return ss.str();
    }

    uint160 Credit::KeyHash() const
    {
        return ::KeyHash(public_key);
    }

    vch_t Credit::getvch() const
    {
        vch_t output;
        output.resize(8);
        memcpy(&output[0], &amount, 8);
        vch_t key_bytes = public_key.getvch();
        output.insert(output.end(), key_bytes.begin(), key_bytes.end());
        return output;
    }

    bool Credit::operator==(const Credit& other_credit) const
    {
        return amount == other_credit.amount and public_key == other_credit.public_key;
    }

    bool Credit::operator<(const Credit& other_credit) const
    {
        return amount < other_credit.amount
                or (amount == other_credit.amount and public_key.getvch() < other_credit.public_key.getvch());
    }

/*
 *  Credit
 ************/