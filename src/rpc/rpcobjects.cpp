#include "rpc/rpcobjects.h"
#include "flexnode/flexnode.h"


#include "log.h"
#define LOG_CATEGORY "rpcobjects.cpp"

using namespace json_spirit;

Object GetObjectFromTrade(uint160 accept_commit_hash)
{
    Object trade_object;
    AcceptCommit accept_commit;
    accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    Order order = msgdata[accept_commit.order_hash]["order"];
    string currency(order.currency.begin(), order.currency.end());
    trade_object.push_back(Pair("currency", currency));
    trade_object.push_back(Pair("size", order.size));
    uint8_t side;
    if (tradedata[accept_commit.order_hash]["is_mine"])
    {
        side = order.side;
        trade_object.push_back(Pair("side", side == BID ? "buy" : "sell"));
    }
    else if (tradedata[accept_commit_hash]["is_mine"])
    {
        side = 1 - order.side;
        trade_object.push_back(Pair("side", side == BID ? "buy" : "sell"));
    }
    trade_object.push_back(Pair("integer_price", order.integer_price));

    if (tradedata[accept_commit_hash]["completed"])
        trade_object.push_back(Pair("status", "completed"));
    else if (tradedata[accept_commit_hash]["cancelled"])
        trade_object.push_back(Pair("status", "cancelled"));
    else
        trade_object.push_back(Pair("status", "in progress"));
    return trade_object;
}


Object GetObjectFromOrder(Order order)
{
    Object order_object;
    string currency(order.currency.begin(), order.currency.end());
    order_object.push_back(Pair("currency", currency));
    order_object.push_back(Pair("size", order.size));
    
    order_object.push_back(Pair("side", order.side == BID ? "bid" : "ask"));
    order_object.push_back(Pair("integer_price", order.integer_price));

    order_object.push_back(Pair("is_cryptocurrency",
                                order.is_cryptocurrency ? "true": "false"));
    order_object.push_back(Pair("fund_address_pubkey", 
                                order.fund_address_pubkey.ToString()));
    order_object.push_back(Pair("relay_chooser_hash",
                                order.relay_chooser_hash.ToString()));

    if (order.auxiliary_data.size() > 0)
    {
        string auxiliary_data_string(order.auxiliary_data.begin(),
                                     order.auxiliary_data.end());
        order_object.push_back(Pair("auxiliary_data", auxiliary_data_string));
    }
    string fund_proof(order.fund_proof.begin(), order.fund_proof.end());

    order_object.push_back(Pair("fund_proof", fund_proof));
    order_object.push_back(Pair("timestamp", order.timestamp));
    order_object.push_back(Pair("timeout", (uint64_t)order.timeout));
    return order_object;
}

string OrderStatus(uint160 order_hash)
{
    uint160 accept_commit_hash;
    if (tradedata[order_hash].HasProperty("accept_commit_hash"))
    {
        accept_commit_hash = tradedata[order_hash]["accept_commit_hash"];
        
        if (tradedata[accept_commit_hash]["completed"])
            return "completed";
        else if (tradedata[accept_commit_hash]["cancelled"])
            return "trade cancelled";
        else
            return "trade in progress";
    }
    else if (tradedata[order_hash]["cancelled"])
        return "cancelled";
    else
        return "active";
}

Object GetObjectFromMinedCredit(MinedCredit credit)
{
    Object mined_credit_object;
    mined_credit_object.push_back(Pair("hash", credit.GetHash160().ToString()));
    mined_credit_object.push_back(Pair("previous_mined_credit_hash",
                                       credit.previous_mined_credit_hash.ToString()));
    mined_credit_object.push_back(Pair("previous_diurn_root",
                                       credit.previous_diurn_root.ToString()));
    mined_credit_object.push_back(Pair("diurnal_block_root",
                                       credit.diurnal_block_root.ToString()));
    mined_credit_object.push_back(Pair("batch_root",
                                       credit.batch_root.ToString()));
    mined_credit_object.push_back(Pair("spent_chain_hash",
                                       credit.spent_chain_hash.ToString()));
    mined_credit_object.push_back(Pair("relay_state_hash",
                                       credit.relay_state_hash.ToString()));
    mined_credit_object.push_back(Pair("total_work",
                                       credit.total_work.ToString()));
    mined_credit_object.push_back(Pair("batch_number",
                                       (uint64_t)credit.batch_number));
    mined_credit_object.push_back(Pair("offset",
                                       (uint64_t)credit.offset));
    mined_credit_object.push_back(Pair("timestamp",
                                       credit.timestamp));
    mined_credit_object.push_back(Pair("difficulty",
                                       credit.difficulty.ToString()));
    mined_credit_object.push_back(Pair("diurnal_difficulty",
                                       credit.diurnal_difficulty.ToString()));
    return mined_credit_object;
}

Object GetObjectFromCalendar(Calendar calendar)
{
    Object calendar_object;
    Array calends;
    Array current_diurn;

    for (uint32_t i = 0; i < calendar.calends.size(); i++)
    {
        Calend calend = calendar.calends[i];
        CreditAndProofList top_up_work = calendar.extra_work[i];
        Array extra_work;
        Object calend_object;
        calend_object.push_back(Pair("mined_credit_hash",
                                     calend.MinedCreditHash().ToString()));
        calend_object.push_back(Pair("diurnal_difficulty",
                                     calend.mined_credit.diurnal_difficulty.ToString()));

        foreach_(CreditAndProof& credit_and_proof, top_up_work)
        {
            Object extra_work_object;
            MinedCredit credit = credit_and_proof.first;
            extra_work_object.push_back(Pair("mined_credit_hash",
                                             credit.GetHash160().ToString()));
            extra_work_object.push_back(Pair("difficulty",
                                             credit.difficulty.ToString()));
            extra_work.push_back(extra_work_object);
        }
        calend_object.push_back(Pair("top_up_work", extra_work));
        calends.push_back(calend_object);
    }
    calendar_object.push_back(Pair("calends", calends));
    foreach_(MinedCreditMessage& msg, calendar.current_diurn.credits_in_diurn)
    {
        Object diurn_entry;
        diurn_entry.push_back(Pair("mined_credit_hash",
                                   msg.mined_credit.GetHash160().ToString()));
        diurn_entry.push_back(Pair("difficulty",
                                   msg.mined_credit.difficulty.ToString()));
        current_diurn.push_back(diurn_entry);
    }
    calendar_object.push_back(Pair("current_diurn", current_diurn));
    return calendar_object;
}

Object GetObjectFromCredit(Credit credit)
{
    Object credit_object;
    uint160 keyhash;
    Point pubkey;
    if (credit.keydata.size() == 20)
    {
        keyhash = uint160(credit.keydata);
    }
    else
    {
        pubkey.setvch(credit.keydata);
        credit_object.push_back(Pair("pubkey", pubkey.ToString()));
        keyhash = KeyHash(pubkey);
    }

    credit_object.push_back(Pair("keyhash", keyhash.ToString()));
    credit_object.push_back(Pair("amount", credit.amount));
    return credit_object;
}

Object GetObjectFromCreditInBatch(CreditInBatch credit)
{
    Object credit_object;
    uint160 keyhash;
    Point pubkey;
    if (credit.keydata.size() == 20)
    {
        keyhash = uint160(credit.keydata);
    }
    else
    {
        pubkey.setvch(credit.keydata);
        credit_object.push_back(Pair("pubkey", pubkey.ToString()));
        keyhash = KeyHash(pubkey);
    }

    credit_object.push_back(Pair("keyhash", keyhash.ToString()));
    credit_object.push_back(Pair("amount", credit.amount));
    credit_object.push_back(Pair("position", credit.position));

    bool spent = flexnode.spent_chain.Get(credit.position);
    credit_object.push_back(Pair("spent", spent));
    
    Array branch, diurn_branch;

    foreach_(uint160 hash, credit.branch)
        branch.push_back(hash.ToString());
    foreach_(uint160 hash, credit.diurn_branch)
        diurn_branch.push_back(hash.ToString());
    
    credit_object.push_back(Pair("branch", branch));
    credit_object.push_back(Pair("diurn_branch", diurn_branch));

    return credit_object;
}

Object GetObjectFromBatch(CreditBatch batch)
{
    Object batch_object;
    Array credits;

    foreach_(Credit& credit, batch.credits)
    {
        CreditInBatch credit_ = batch.GetWithPosition(credit);
        credits.push_back(GetObjectFromCreditInBatch(credit_));
    }
    batch_object.push_back(Pair("batch_root", batch.Root().ToString()));
    batch_object.push_back(Pair("credits", credits));
    batch_object.push_back(Pair("offset", batch.tree.offset));
    batch_object.push_back(Pair("previous_credit_hash",
                                batch.previous_credit_hash.ToString()));
    return batch_object;
}

Object GetObjectFromMinedCreditMessage(MinedCreditMessage& msg)
{
    Object msg_object;
    Array contents;
    msg_object.push_back(Pair("mined_credit_hash",
                              msg.mined_credit.GetHash160().ToString()));
    msg_object.push_back(Pair("timestamp",
                              msg.timestamp));
    msg.hash_list.RecoverFullHashes();

    foreach_(uint160 hash, msg.hash_list.full_hashes)
    {
        Object msg_hash_object;
        string_t type = msgdata[hash]["type"];
        msg_hash_object.push_back(Pair("type", type));
        msg_hash_object.push_back(Pair("hash", hash.ToString()));
        contents.push_back(msg_hash_object);
    }
    msg_object.push_back(Pair("contents", contents));
    return msg_object;
}

Object GetObjectFromSignature(Signature signature)
{
    Object object;
    object.push_back(Pair("exhibit", signature.exhibit.ToString()));
    object.push_back(Pair("signature", signature.signature.ToString()));
    return object;
}

Object GetObjectFromTransaction(SignedTransaction tx)
{
    Object transaction;
    Array inputs;
    Array outputs;

    transaction.push_back(Pair("hash", tx.GetHash160().ToString()));

    Object signature = GetObjectFromSignature(tx.signature);
    transaction.push_back(Pair("signature", signature));

    foreach_(CreditInBatch& credit, tx.rawtx.inputs)
    {
        inputs.push_back(GetObjectFromCreditInBatch(credit));
    }
    foreach_(Credit& credit, tx.rawtx.outputs)
    {
        outputs.push_back(GetObjectFromCredit(credit));
    }

    transaction.push_back(Pair("inputs", inputs));
    transaction.push_back(Pair("outputs", outputs));
    return transaction;
}

Object GetObjectFromRelayState(RelayState state)
{
    Object object;
    Array relays;

    foreach_(const Numbering::value_type& item, state.relays)
    {
        Object relay;
        Point relay_pubkey = item.first;
        relay.push_back(Pair("relay_number", (uint64_t)item.second));
        relay.push_back(Pair("relay_pubkey", relay_pubkey.ToString()));
        bool disqualified = state.disqualifications.count(relay_pubkey);
        relay.push_back(Pair("disqualified", disqualified));
        relays.push_back(relay);
    }

    object.push_back(Pair("relays", relays));
    object.push_back(Pair("latest_credit_hash",
                          state.latest_credit_hash.ToString()));

    return object;
}

Object GetObjectFromJoin(RelayJoinMessage join)
{
    Object object;

    object.push_back(Pair("relay_number", (uint64_t)join.relay_number));
    object.push_back(Pair("credit_hash", join.credit_hash.ToString()));
    object.push_back(Pair("preceding_relay_join_hash",
                          join.preceding_relay_join_hash.ToString()));
    object.push_back(Pair("relay_pubkey",
                          join.relay_pubkey.ToString()));
    object.push_back(Pair("successor_secret_xor_shared_secret",
                          join.successor_secret_xor_shared_secret.ToString()));
    object.push_back(Pair("successor_secret_point",
                          join.successor_secret_point.ToString()));
    object.push_back(Pair("distributed_succession_secret",
                          join.distributed_succession_secret.ToString()));
    object.push_back(Pair("signature",
                          GetObjectFromSignature(join.signature)));

    return object;
}

Object GetObjectFromSuccession(SuccessionMessage succession)
{
    Object object;

    object.push_back(Pair("dead_relay_join_hash",
                          succession.dead_relay_join_hash.ToString()));
    object.push_back(Pair("successor_join_hash",
                          succession.successor_join_hash.ToString()));
    object.push_back(Pair("executor", succession.executor.ToString()));
    object.push_back(
        Pair("succession_secret_xor_shared_secret",
             succession.succession_secret_xor_shared_secret.ToString()));
    object.push_back(Pair("signature",
                          GetObjectFromSignature(succession.signature)));

    return object;
}