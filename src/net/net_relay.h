#ifndef FLEX_NET_RELAY_H
#define FLEX_NET_RELAY_H


#include <src/credits/MinedCreditMessage.h>
#include <src/credits/SignedTransaction.h>
#include "net_cnode.h"

void RelayMinedCredit(const MinedCreditMessage& message);
void RelayMinedCredit(const MinedCreditMessage& message,
                      const uint256& hash, const CDataStream& ss);


void RelayTransaction(const SignedTransaction& tx);
void RelayTransaction(const SignedTransaction& tx, const uint256& hash);
void RelayTransaction(const SignedTransaction& tx,
                      const uint256& hash, const CDataStream& ss);

void RelayMessage(const CDataStream& ss, int type);

void RelayTradeMessage(const CDataStream& ss);

void RelayDepositMessage(const CDataStream& ss);

void RelayRelayMessage(const CDataStream& ss);

void TellNodeAboutTransaction(CNode* pnode,
                              const SignedTransaction& tx);

#endif //FLEX_NET_RELAY_H
