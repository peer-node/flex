#include "trade/trade.h"
#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "relay.cpp"


bool CheckNewRelayChoiceMessage(NewRelayChoiceMessage msg)
{
    if (tradedata[msg.accept_commit_hash]["cancelled"])
        return false;

    if (!InMainChain(msg.credit_hash))
        return false;

    uint32_t latest_batch_number = LatestMinedCredit().batch_number;

    MinedCredit specified_mined_credit;

    specified_mined_credit = creditdata[msg.credit_hash]["mined_credit"];
    if (latest_batch_number - specified_mined_credit.batch_number > 10)
        return false;

    return msg.VerifySignature();
}


void CheckForRelayDutiesAfterNewRelayChoice(NewRelayChoiceMessage msg)
{
    uint160 accept_commit_hash = msg.accept_commit_hash;
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];

    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    Order order = msgdata[commit.order_hash]["order"];

    bool chooser_placed_order = msg.VerificationKey() 
                                == commit.VerificationKey();

    DistributedTradeSecret secret = chooser_placed_order
                                    ? accept_commit.distributed_trade_secret
                                    : commit.distributed_trade_secret;
    
    bool secrets_to_send = false;
    Point key;
    vector<Point> relays = msg.Relays();

    for (uint32_t i = 0; i < relays.size(); i++)
    {
        if (!keydata[relays[i]].HasProperty("privkey"))
            continue;

        PieceOfSecret piece = secret.pieces[i];
        
        CBigNum privkey = keydata[relays[i]]["privkey"];

        CBigNum shared_secret = Hash(privkey * piece.credit_pubkey);
        CBigNum credit_secret = shared_secret ^
                                piece.credit_secret_xor_shared_secret;

        if (piece.credit_pubkey != Point(SECP256K1, credit_secret))
        {
            log_ << "Bad original credit secret!!\n";
            continue;
        }

        keydata[piece.credit_pubkey]["privkey"] = credit_secret;
        
        CBigNum currency_secret = Hash(credit_secret) ^      
                            piece.currency_secret_xor_hash_of_credit_secret;

        if (piece.currency_pubkey != Point(piece.currency_pubkey.curve,
                                           currency_secret))
        {
            if (msg.chooser_side == BID)
            {
                log_ << "Bad original currency secret!!\n";
                continue;
            }
        }
        else
            keydata[piece.currency_pubkey]["privkey"] = currency_secret;

        secrets_to_send = true;
    }
    if (secrets_to_send)
        SendSecrets(accept_commit_hash, FORWARD);
}

void HandleNewRelayChoiceMessage(NewRelayChoiceMessage msg)
{
    if (keydata[msg.VerificationKey()].HasProperty("privkey"))
        return;
    if (!CheckNewRelayChoiceMessage(msg))
        return;
    if (tradedata[msg.accept_commit_hash]["is_mine"])
    {
        if (tradedata[msg.accept_commit_hash]["new_relay_choice_handled"])
            return;
        if ((msg.chooser_side == ASK &&
             tradedata[msg.accept_commit_hash]["my_buy"]) ||
            (msg.chooser_side == BID &&
             tradedata[msg.accept_commit_hash]["my_sell"]))
        {
            NewRelayChoiceMessage my_msg(msg.accept_commit_hash,
                                         msg.credit_hash,
                                         1 - msg.chooser_side);
            if (my_msg.positions != msg.positions)
                return;
            flexnode.tradehandler.BroadcastMessage(my_msg);
        }
        
        tradedata[msg.accept_commit_hash]["new_relay_choice_handled"] = true;
    }
    CheckForRelayDutiesAfterNewRelayChoice(msg);
}


void ScheduleSendBackupSecrets(uint160 accept_commit_hash, uint8_t direction)
{
    flexnode.scheduler.Schedule("send_backup_secrets", 
                                accept_commit_hash,
                                GetTimeMicros() + SEND_BACKUP_SECRETS_AFTER);
}

void SendSecrets(uint160 accept_commit_hash, uint8_t direction)
{
    if (tradedata[accept_commit_hash]["cancelled"] ||
        tradedata[accept_commit_hash]["i_sent_secrets"])
        return;
    log_ << "SendSecrets(): " << accept_commit_hash 
         << " direction: " << direction << "\n";
    
    uint32_t num_secrets;
    
    tradedata[accept_commit_hash]["direction"] = direction;

    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    
    vector<Point> relays = accept_commit.distributed_trade_secret.Relays();
    num_secrets = relays.size();

    for (uint32_t position = 0; position < num_secrets; position++)
    {
        Point relay_pubkey = relays[position];
        if (keydata[relay_pubkey].HasProperty("privkey"))
        {
            log_ << "sending secret for position " << position << "\n";
            SendSecret(commit, accept_commit, position, direction);
        }
    }

    if (direction == BACKWARD)
        tradedata[accept_commit_hash]["cancelled"] = true;
    else
        tradedata[accept_commit_hash]["i_sent_secrets"] = true;

    ScheduleSendBackupSecrets(accept_commit_hash, direction);
}


void SendSecret(OrderCommit commit,
                AcceptCommit accept_commit,
                uint32_t position,
                uint8_t direction)
{
    CBigNum credit_secret, currency_secret;
    DistributedTradeSecret secret_for_credit_recipient, 
                           secret_for_currency_recipient;

    Point credit_recipient, currency_recipient;

    Order order = msgdata[accept_commit.order_hash]["order"];


    if ((direction == FORWARD && order.side == BID) ||
        (direction == BACKWARD && order.side == ASK))
    {
        credit_recipient = accept_commit.VerificationKey();
        secret_for_credit_recipient = commit.distributed_trade_secret;
        currency_recipient = commit.VerificationKey();
        secret_for_currency_recipient
            = accept_commit.distributed_trade_secret;
    }
    else if ((direction == FORWARD && order.side == ASK) ||
             (direction == BACKWARD && order.side == BID))
    {
        credit_recipient = commit.VerificationKey();
        secret_for_credit_recipient
            = accept_commit.distributed_trade_secret;
        currency_recipient = accept_commit.VerificationKey();
        secret_for_currency_recipient = commit.distributed_trade_secret;
    }
    
    Point credit_pubkey
        = secret_for_credit_recipient.pieces[position].credit_pubkey;
    Point currency_pubkey 
        = secret_for_currency_recipient.pieces[position].currency_pubkey;

    credit_secret = keydata[credit_pubkey]["privkey"];
    currency_secret = keydata[currency_pubkey]["privkey"];

    log_ << "SendSecret():\n"
         << "credit secret = " << credit_secret << "  "
         << "currency secret = " << currency_secret << "\n";

    SendTradeSecretMessage(accept_commit.GetHash160(),
                           direction,
                           credit_secret,
                           currency_secret,
                           credit_recipient,
                           currency_recipient,
                           position);
}

void SendTradeSecretMessage(uint160 accept_commit_hash,
                            uint8_t direction,
                            CBigNum credit_secret,
                            CBigNum currency_secret,
                            Point credit_recipient,
                            Point currency_recipient,
                            uint32_t position)
{
    CBigNum shared_secret = Hash(credit_secret * credit_recipient);
    CBigNum other_shared_secret = Hash(currency_secret * currency_recipient);

    log_ << "SendTradeSecretMessage(): " << accept_commit_hash
         << "  position: " << position << "  direction:" << direction << "\n"
         << "shared secret = " << shared_secret << "  "
         << "other shared secret = " << other_shared_secret << "\n";

    TradeSecretMessage message(accept_commit_hash,
                               direction,
                               credit_secret ^ shared_secret,
                               currency_secret ^ other_shared_secret,
                               position);

    if (keydata[message.VerificationKey()].HasProperty("privkey"))
    {
        message.Sign();
        tradedata[message.GetHash160()]["is_mine"] = true;
        flexnode.tradehandler.BroadcastMessage(message);
    }
    else
    {
        BackupTradeSecretMessage message(accept_commit_hash,
                                         direction,
                                         credit_secret ^ shared_secret,
                                         currency_secret ^ other_shared_secret,
                                         position);
        message.Sign();
        tradedata[message.GetHash160()]["is_mine"] = true;
        flexnode.tradehandler.BroadcastMessage(message);
    }
}

void ScheduleCancellation(uint160 complaint_hash)
{
    flexnode.scheduler.Schedule("cancellation", complaint_hash, 
                                GetTimeMicros() + COMPLAINT_WAIT_TIME);
}

void DoScheduledCancellation(uint160 complaint_hash)
{
    TraderComplaint msg = msgdata[complaint_hash]["trader_complaint"];
    TradeSecretMessage secret = msgdata[msg.message_hash]["secret"];

    if (tradedata[complaint_hash]["refuted"])
    {
        log_ << "complaint " << complaint_hash << " was refuted\n";
        SendSecrets(secret.accept_commit_hash, FORWARD);
        return;
    }
    SendSecrets(secret.accept_commit_hash, BACKWARD);
}

void DoScheduledTimeout(uint160 accept_commit_hash)
{
    if (tradedata[accept_commit_hash]["bid_counterparty_secret_sent"] &&
        (tradedata[accept_commit_hash]["ask_counterparty_secret_sent"] ||
         tradedata[accept_commit_hash]["fiat_trade"]))
    {
        log_ << "Secrets already sent. Not sending\n";
        return;
    }
    if (tradedata[accept_commit_hash]["cancelled"] &&
        tradedata[accept_commit_hash]["i_sent_secrets"])
        return;

    if (!tradedata[accept_commit_hash]["i_have_secret"])
        return;

    SendSecrets(accept_commit_hash, BACKWARD);
}




/*************************
 *  TradeHandler
 */

    void TradeHandler::HandleRefutationOfRelayComplaint(
        RefutationOfRelayComplaint refutation)
    {
        uint160 refutation_hash = refutation.GetHash160();
        
        if (!refutation.VerifySignature() || !refutation.Validate())
        {
            should_forward = false;
            return;
        }
        tradedata[refutation.complaint_hash]["refuted"] = true;
        flexnode.pit.HandleRelayMessage(refutation_hash);
    }

    void TradeHandler::HandleTraderComplaint(TraderComplaint complaint)
    {
        uint160 message_hash = complaint.message_hash;
        
        if (!CheckTraderComplaint(complaint))
        {
            should_forward = false;
            return;
        }

        uint160 complaint_hash = complaint.GetHash160();
        
        TradeSecretMessage msg = msgdata[message_hash]["secret"];

        pair<string_t, Point> complaint_key;
        complaint_key = make_pair("received_complaint_about", 
                                  msg.VerificationKey());

        tradedata[msg.accept_commit_hash][complaint_key] = true;

        vector<Point> awaited_relays = tradedata[msg.accept_commit_hash]["awaited_relays"];
        awaited_relays.push_back(msg.VerificationKey());
        tradedata[msg.accept_commit_hash]["awaited_relays"] = awaited_relays;

        flexnode.pit.HandleRelayMessage(message_hash);

        if (tradedata[message_hash]["is_mine"])
        {
            BroadcastMessage(RefutationOfTraderComplaint(complaint_hash));
        }
    }

    void TradeHandler::HandleRefutationOfTraderComplaint(
        RefutationOfTraderComplaint refutation)
    {
        uint160 refutation_hash = refutation.GetHash160();
        
        if (!refutation.VerifySignature() || !refutation.Validate())
        {
            should_forward = false;
            return;
        }
        tradedata[refutation.complaint_hash]["refuted"] = true;
        flexnode.pit.HandleRelayMessage(refutation_hash);

        uint160 accept_commit_hash = refutation.GetAcceptCommitHash();

        vector<Point> awaited_relays = tradedata[accept_commit_hash]["awaited_relays"];
        EraseEntryFromVector(refutation.VerificationKey(), awaited_relays);
        tradedata[accept_commit_hash]["awaited_relays"] = awaited_relays;
    }

    bool TradeHandler::CheckForMyRelaySecrets(AcceptCommit accept_commit)
    {
        uint160 accept_commit_hash = accept_commit.GetHash160();
        OrderCommit commit;
        commit = msgdata[accept_commit.order_commit_hash]["commit"];

        DistributedTradeSecret secret = accept_commit.distributed_trade_secret;

        bool ok = true;

        ok &= ValidateAndStoreSecrets(secret, accept_commit_hash);

        secret = commit.distributed_trade_secret;

        ok &= ValidateAndStoreSecrets(secret, accept_commit.order_commit_hash);

        return ok;
    }

    bool TradeHandler::ValidateAndStoreSecrets(
        DistributedTradeSecret secret, 
        uint160 message_hash)
    {
        bool ok = true;
        BigNumsInPositions bad_shared_secrets,
                           credit_secrets_that_dont_recover_currency_secrets;
        
        if (!secret.ValidateAndStoreMySecretPieces(
                        bad_shared_secrets,
                        credit_secrets_that_dont_recover_currency_secrets))
        {
            ok = false;
            foreach_(const BigNumsInPositions::value_type& item,
                     bad_shared_secrets)
            {
                uint32_t position = item.first;
                CBigNum bad_shared_secret = item.second;
                RelayComplaint complaint(message_hash,
                                         BID,
                                         position,
                                         bad_shared_secret);
                BroadcastMessage(complaint);
            }
            foreach_(const BigNumsInPositions::value_type& item,
                     credit_secrets_that_dont_recover_currency_secrets)
            {
                uint32_t position = item.first;
                CBigNum bad_credit_secret = item.second;
                RelayComplaint complaint(message_hash,
                                         ASK,
                                         position,
                                         bad_credit_secret);
                BroadcastMessage(complaint);
            }
        }
        else
        {
            tradedata[message_hash]["i_have_secret"] = true;
        }
        return ok;
    }

    bool TradeHandler::CheckDisclosure(SecretDisclosureMessage msg)
    {
        if (!msg.VerifySignature() || !msg.disclosure.Validate())
        {
            should_forward = false;
            return false;
        }

        std::vector<Point> relays = msg.disclosure.Relays();

        DistributedTradeSecret secret;

        AcceptCommit accept_commit
            = msgdata[msg.accept_commit_hash]["accept_commit"];
        OrderCommit commit
            = msgdata[accept_commit.order_commit_hash]["commit"];
        Order order = msgdata[commit.order_hash]["order"];


        uint160 original_msg_hash;
        if (msg.side == order.side)
        {
            secret = commit.distributed_trade_secret;
            original_msg_hash = accept_commit.order_commit_hash;
        }
        else
        {
            secret = accept_commit.distributed_trade_secret;
            original_msg_hash = msg.accept_commit_hash;
        }

        BigNumsInPositions bad_revelation_shared_secrets;
        BigNumsInPositions bad_currency_secrets;
        BigNumsInPositions bad_original_shared_secrets;

        if (!msg.disclosure.ValidateAndStoreMyRevelations(
                                        secret,
                                        bad_revelation_shared_secrets,
                                        bad_currency_secrets,
                                        bad_original_shared_secrets))
        {
            uint160 hash = msg.GetHash160();

            foreach_(BigNumsInPositions::value_type& item,
                     bad_revelation_shared_secrets)
            {
                uint32_t position = item.first;
                CBigNum bad_shared_secret = item.second;
                RelayComplaint complaint(hash, BID, 
                                         position, bad_shared_secret);
                BroadcastMessage(complaint);
            }
            foreach_(BigNumsInPositions::value_type& item,
                     bad_currency_secrets)
            {
                uint32_t position = item.first;
                CBigNum bad_shared_secret = item.second;
                RelayComplaint complaint(hash, ASK, 
                                         position, bad_shared_secret);
                BroadcastMessage(complaint);
            }
            foreach_(BigNumsInPositions::value_type& item,
                     bad_original_shared_secrets)
            {
                uint32_t position = item.first;
                CBigNum bad_shared_secret = item.second;
                RelayComplaint complaint(hash, BID, 
                                         position, bad_shared_secret);
                BroadcastMessage(complaint);
            }
            return false;
        }
        return true;
    }

    void TradeHandler::HandleSecretDisclosureMessage(
        SecretDisclosureMessage msg)
    {
        log_ << "TradeHandler(): handling " << msg;
        std::pair<string_t, uint8_t> disclosure_key;
        disclosure_key = make_pair(string_t("disclosure"), msg.side);

        if (tradedata[msg.accept_commit_hash].HasProperty(disclosure_key))
        {
            log_ << "already have this disclosure: " << disclosure_key
                 << "\n";
            return;
        }

        if (!CheckDisclosure(msg))
        {
            log_ << "Disclosure failed check!\n";
            return;
        }
        
        tradedata[msg.accept_commit_hash][disclosure_key] = msg;
        if (tradedata[msg.accept_commit_hash]["is_mine"])
        {
            AcceptCommit accept_commit;
            accept_commit = msgdata[msg.accept_commit_hash]["accept_commit"];
            Order order = msgdata[accept_commit.order_hash]["order"];
            log_ << "Trade is mine. Responding with disclosure\n";
            SendDisclosure(msg.accept_commit_hash, accept_commit, order, true);
            log_ << "and scheduling initiation of trade\n";
            ScheduleTradeInitiation(msg.accept_commit_hash);
        }
        else
        {
            log_ << "Trade isn't mine. Not responding with discloure\n";
        }

        foreach_(Point relay, msg.disclosure.Relays())
        {
            if (keydata[relay].HasProperty("privkey"))
            {
                tradedata[msg.accept_commit_hash]["have_backup_secret"] = true;
                log_ << "Received backup secret from disclosure\n";
            }
        }

        RecordRelayTradeTasks(msg.accept_commit_hash);
    }

/*
 *  TradeHandler
 *************************/


void RecordRelayTradeTasks(uint160 accept_commit_hash)
{
    uint64_t now = GetTimeMicros();

    if (tradedata[accept_commit_hash]["duties_recorded"])
        return;

    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    
    foreach_(const Point& relay,
             accept_commit.distributed_trade_secret.Relays())
    {
        taskdata[accept_commit_hash].Location(relay) = now;
    }

    tradedata[accept_commit_hash]["awaited_relays"]
        = accept_commit.distributed_trade_secret.Relays();

    tradedata[accept_commit_hash]["duties_recorded"] = true;
}



void DoScheduledSecretsReleaseCheck(uint160 accept_commit_hash)
{
    log_ << "doing secret release check - " << accept_commit_hash << "\n";
    if (!tradedata[accept_commit_hash]["i_have_secret"])
    {
        log_ << "no secrets!\n";
        return;
    }

    if (!CheckForCreditPayment(accept_commit_hash))
    {
        log_ << "no credit payment yet - scheduling another check\n";
        flexnode.scheduler.Schedule("secrets_release_check", 
                                      accept_commit_hash, 
                                      GetTimeMicros() + 5 * 1000 * 1000);
        return;
    }
    log_ << "Credit payment made!\n";
    if (!tradedata[accept_commit_hash]["currency_paid"])
    {
        log_ << "currency not paid. scheduling another check\n";
        flexnode.scheduler.Schedule("secrets_release_check", 
                                      accept_commit_hash, 
                                      GetTimeMicros() + 5 * 1000 * 1000);
        return;
    }

    if (tradedata[accept_commit_hash]["bid_counterparty_secret_sent"] &&
        (tradedata[accept_commit_hash]["ask_counterparty_secret_sent"] ||
         tradedata[accept_commit_hash]["fiat_trade"]))
    {
        log_ << "Secrets already sent. Not sending\n";
        return;
    }

    uint64_t currency_payment_time
        = tradedata[accept_commit_hash]["confirmation_time"];
    uint64_t credit_payment_time
        = tradedata[accept_commit_hash]["credit_pay_time"];

    uint64_t when_both_were_paid
        = currency_payment_time > credit_payment_time
          ? currency_payment_time : credit_payment_time;

    if (GetTimeMicros() - when_both_were_paid > SEND_RELAY_SECRETS_AFTER)
    {        
        log_ << "sending secrets\n";
        SendSecrets(accept_commit_hash, FORWARD);
        tradedata[accept_commit_hash]["secrets_sent"] = true;
        return;
    }
    log_ << "too early to send\n";
    flexnode.scheduler.Schedule("secrets_release_check", 
                                  accept_commit_hash, 
                                  when_both_were_paid
                                   + SEND_RELAY_SECRETS_AFTER);
}

void DoScheduledSecretsReleasedCheck(uint160 accept_commit_hash)
{
    log_ << "DoScheduledSecretsReleasedCheck: "
         << accept_commit_hash << "\n";
    vector<Point> awaited_relays = tradedata[accept_commit_hash]["awaited_relays"];
    foreach_(Point non_responder, awaited_relays)
    {
        log_ << non_responder << " is not responding - doing succession\n";
        DoSuccession(non_responder);
    }
}

void DoScheduledBackupSecretsReleaseCheck(uint160 accept_commit_hash)
{
    log_ << "DoScheduledBackupSecretsReleaseCheck()\n";
    if (tradedata[accept_commit_hash]["sent_backup_secrets"])
        return;
    uint8_t direction = tradedata[accept_commit_hash]["direction"];
    AcceptCommit accept_commit = msgdata[accept_commit_hash]["accept_commit"];
    OrderCommit commit = msgdata[accept_commit.order_commit_hash]["commit"];
    vector<Point> relays = accept_commit.distributed_trade_secret.Relays();

    for (uint32_t i = 0; i < relays.size(); i++)
    {
        log_ << "secret: " << i << "\n";
        std::pair<string_t, Point> key
            = make_pair("received_secret_from", relays[i]);
        std::pair<string_t, Point> complaint_key
            = make_pair("received_complaint_about", relays[i]);
        
        if (tradedata[accept_commit_hash][key] &&
            !tradedata[accept_commit_hash][complaint_key])
        {
            log_ << "secret has been sent by primary relay\n";
            continue;
        }
        log_ << "sending\n";
        SendSecret(commit, accept_commit, i, direction);
    }
    tradedata[accept_commit_hash]["sent_backup_secrets"] = true;
}

