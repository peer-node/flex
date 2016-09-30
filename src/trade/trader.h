#ifndef TELEPORT_TRADER
#define TELEPORT_TRADER

#include "trade/trade_messages.h"
#include "teleportnode/orphanage.h"
#include "teleportnode/schedule.h"
#include "credits/creditutil.h"

std::pair<uint8_t,vch_t> BidSide(vch_t currency);

std::pair<uint8_t,vch_t> AskSide(vch_t currency);



void ScheduleSecretsCheck(uint160 accept_commit_hash, uint32_t seconds);

void DoScheduledSecretsCheck(uint160 accept_commit_hash);

void CompleteTransaction(uint160 accept_commit_hash,
                         DistributedTradeSecret dist_secret,
                         uint8_t side,
                         uint8_t direction);

void CompleteTransactionAfterRecoveringKey(uint160 accept_commit_hash,
                                           uint8_t side,
                                           uint8_t direction);

void ImportPrivKeyAfterTransaction(CBigNum privkey,
                                   uint160 accept_commit_hash,
                                   uint8_t side, uint8_t direction);

uint32_t IncrementAndCountSecretsReceived(uint160 accept_commit_hash,
                                          uint8_t side);


Point GetFundAddressPubKey(AcceptCommit accept_commit, uint8_t side);


Point GetEscrowCreditPubKey(OrderCommit order_commit, 
                            AcceptCommit accept_commit);

Point GetEscrowCreditPubKey(uint160 accept_comit_hash);

Point GetEscrowCurrencyPubKey(OrderCommit order_commit, 
                                       AcceptCommit accept_commit);

Point GetEscrowCurrencyPubKey(uint160 accept_comit_hash);

CBigNum GetEscrowCreditPrivKey(OrderCommit order_commit, 
                               AcceptCommit accept_commit);

CBigNum GetEscrowCreditPrivKey(uint160 accept_comit_hash);

CBigNum GetEscrowCurrencyPrivKey(OrderCommit order_commit, 
                               AcceptCommit accept_commit);

CBigNum GetEscrowCurrencyPrivKey(uint160 accept_comit_hash);

vch_t SendCredit(AcceptCommit accept_commit, Order order);

vch_t SendCurrency(AcceptCommit accept_commit, Order order);

void ReleaseCreditToCounterParty(uint160 accept_commit_hash);

void ReleaseCurrencyToCounterParty(uint160 accept_commit_hash);

Point ForwardPubKeyOfDistributedSecret(uint160 accept_commit_hash, 
                                       uint8_t side);

Point GetTraderPubKey(AcceptCommit accept_commit, uint8_t side);

void SendCurrencyPaymentConfirmation(uint160 accept_commit_hash,
                                     uint160 payment_hash);


void DoScheduledTradeInitiation(uint160 accept_commit_hash);

void SendTraderComplaint(uint160 message_hash, 
                         uint8_t side);


#endif