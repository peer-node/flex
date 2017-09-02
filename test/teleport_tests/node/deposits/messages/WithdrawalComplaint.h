#ifndef TELEPORT_WITHDRAWALCOMPLAINT_H
#define TELEPORT_WITHDRAWALCOMPLAINT_H


#include "WithdrawalMessage.h"

class WithdrawalComplaint
{
public:
    uint160 withdraw_msg_hash;
    Signature signature;

    WithdrawalComplaint() { }

    WithdrawalComplaint(uint160 withdraw_msg_hash):
            withdraw_msg_hash(withdraw_msg_hash)
    { }

    static string_t Type() { return string_t("withdraw_complaint"); }

    DEPENDENCIES(withdraw_msg_hash);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(withdraw_msg_hash);
        READWRITE(signature);
    )

    Point GetDepositAddress(Data data)
    {
        return GetWithdraw(data).GetDepositAddress(data);
    }

    WithdrawalMessage GetWithdraw(Data data)
    {
        return data.GetMessage(withdraw_msg_hash);
    }

    Point VerificationKey(Data data)
    {
        return GetWithdraw(data).GetRequest(data).recipient_key;
    }

    IMPLEMENT_HASH_SIGN_VERIFY();
};


#endif //TELEPORT_WITHDRAWALCOMPLAINT_H
