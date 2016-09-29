#include <src/crypto/hashtrees.h>
#include "Calend.h"


Calend::Calend(MinedCreditMessage msg)
{
    hash_list = msg.hash_list;
    proof_of_work = msg.proof_of_work;
    mined_credit = msg.mined_credit;
}

uint160 Calend::DiurnRoot() const
{
    return SymmetricCombine(mined_credit.network_state.previous_diurn_root,
                            mined_credit.network_state.diurnal_block_root);
}

uint160 Calend::TotalCreditWork()
{
    return mined_credit.network_state.previous_total_work + mined_credit.network_state.difficulty;
}

DiurnMessageData Calend::GenerateDiurnMessageData(CreditSystem *credit_system)
{
    DiurnMessageData message_data;


    return message_data;
}
