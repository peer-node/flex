// Distributed under version 3 of the Gnu Affero GPL software license, 
// see the accompanying file COPYING for details.


#include "relays/relay_messages.h"
#include "flexnode/flexnode.h"

#include "log.h"
#define LOG_CATEGORY "relay_messages.cpp"


/**********************************
 *  FutureSuccessorSecretMessage
 */

    FutureSuccessorSecretMessage::FutureSuccessorSecretMessage(
            uint160 credit_hash, 
            Point successor): 
        credit_hash(credit_hash)
    {
        CBigNum successor_secret = keydata[credit_hash]["successor_secret"];
        successor_join_hash = relaydata[successor]["join_msg_hash"];
        CBigNum shared_secret = Hash(successor_secret * successor);

        successor_secret_xor_shared_secret
            = successor_secret ^ shared_secret;
    }

/*
 *  FutureSuccessorSecretMessage
 **********************************/


/**********************
 *  SuccessionMessage
 */

    SuccessionMessage::SuccessionMessage(Point dead_relay, Point executor):
        executor(executor)
    {
        log_ << "SuccessionMessage(): dead: " << dead_relay << "\n"
             << "executor: " << executor << "\n";
        RelayJoinMessage join = GetJoinMessage(dead_relay);
        dead_relay_join_hash = join.GetHash160();
        log_ << "join hash: " << dead_relay_join_hash << "\n";
        log_ << "join is " << join;
        DistributedSuccessionSecret dist_secret
            = join.distributed_succession_secret;
        log_ << "secret:" << dist_secret;
        uint32_t position = dist_secret.RelayPosition(executor);
        log_ << "position: " << position << "\n";
        Point point = dist_secret.point_values[position];
        CBigNum secret = keydata[point]["privkey"];
        log_ << "dead relay: " << dead_relay << "\n";
        Point successor = flexnode.RelayState().Successor(dead_relay);
        log_ << "successor: " << successor << "\n";
        CBigNum shared_secret = Hash(secret * successor);
        succession_secret_xor_shared_secret =  secret ^ shared_secret;
#ifndef _PRODUCTION_BUILD
        log_ << "secret is " << secret << "\n"
             << "shared_secret is " << shared_secret << "\n"
             << "succession_secret_xor_shared_secret is "
             << succession_secret_xor_shared_secret << "\n";
#endif
        successor_join_hash = flexnode.RelayState().GetJoinHash(successor);
    }

/*
 *  SuccessionMessage
 **********************/


 /**********************
 *  RelayLeaveMessage
 */

    RelayLeaveMessage::RelayLeaveMessage(Point relay_leaving):
        relay_leaving(relay_leaving)
    {
        successor = flexnode.RelayState().Successor(relay_leaving);
        
        DistributedSuccessionSecret secret
            = GetDistributedSuccessionSecret(relay_leaving);
        
        CBigNum succession_secret = keydata[secret.PubKey()]["privkey"];
        CBigNum shared_secret = Hash(succession_secret * successor);
        succession_secret_xor_shared_secret 
            = succession_secret ^ shared_secret;
    }

/*
 *  RelayLeaveMessage
 **********************/


/*************************
 *  RelayLeaveComplaint
 */

    Point RelayLeaveComplaint::VerificationKey() const
    {
        RelayLeaveMessage leave = msgdata[leave_msg_hash]["relay_leave"];
        Point relay = leave.VerificationKey();
        return flexnode.RelayState().Successor(relay);
    }

/*
 *  RelayLeaveComplaint
 *************************/
